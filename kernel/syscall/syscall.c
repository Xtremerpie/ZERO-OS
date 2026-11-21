/* ZERO KERNEL - System Call Implementation (Version 2.0.0 System Architecture). */

#include "syscall/syscall.h"
#include "process/process.h"
#include "process/thread.h"
#include "process/scheduler.h"
#include "fs/vfs.h"
#include "ipc/ipc.h"
#include "net/socket.h"
#include "security/sandbox.h"
#include "security/permission.h"
#include "security/capability.h"
#include "security/audit.h"
#include "memory/vmm.h"
#include "zero/cpu.h"
#include "zero/gdt.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"
#include "zero/checked.h"

#define MSR_STAR   0xC0000081u
#define MSR_LSTAR  0xC0000082u
#define MSR_SFMASK 0xC0000084u

extern void syscall_entry(void);
extern uint64_t current_kernel_rsp;

void syscall_init(void)
{
    /* 1. Enable SCE (System Call Extension) bit 0 in EFER */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1ull; /* SCE bit */
    wrmsr(MSR_EFER, efer);

    /* 2. Configure MSR_STAR:
     * Bits 47:32 = Kernel CS (0x08)
     * Bits 63:48 = User CS/SS base selector (0x20: sysret sets SS=0x28, CS=0x30) */
    uint64_t star = ((uint64_t)0x20 << 48) | ((uint64_t)GDT_KERNEL_CS << 32);
    wrmsr(MSR_STAR, star);

    /* 3. Configure MSR_LSTAR with RIP of syscall_entry stub */
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);

    /* 4. Configure MSR_SFMASK: mask IF (bit 9, 0x200) during syscall entry */
    wrmsr(MSR_SFMASK, 0x200ull);

    KLOG_INFO("syscall", "x86_64 fast system call interface active (LSTAR: %p)", syscall_entry);
}

bool syscall_validate_user_ptr(const void *ptr, size_t size)
{
    return sandbox_validate_user_ptr(ptr, size, false);
}

int64_t syscall_dispatch(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    (void)arg4;
    (void)arg5;

    process_t *p = process_get_current();
    uint32_t pid = p ? p->pid : 0;

    /* ZERO Security: Enforce Sandbox Syscall Whitelist */
    if (!sandbox_check_syscall(pid, (uint32_t)num)) {
        audit_log(AUDIT_EVENT_SYSCALL_VIOLATION, pid, "syscall", false, "blocked by sandbox policy");
        return SYS_ERR_PERM;
    }

    switch (num) {
    case SYS_EXIT: {
        int code = (int)arg1;
        process_t *p = process_get_current();
        if (p) {
            process_terminate(p, code);
        }
        thread_exit();
        return SYS_SUCCESS;
    }

    case SYS_YIELD:
        scheduler_yield();
        return SYS_SUCCESS;

    case SYS_SLEEP:
        thread_sleep_ticks(arg1);
        return SYS_SUCCESS;

    case SYS_GETPID: {
        process_t *p = process_get_current();
        return p ? p->pid : 0;
    }

    case SYS_WRITE: {
        int fd = (int)arg1;
        const char *buf = (const char *)(uintptr_t)arg2;
        size_t count = (size_t)arg3;

        if (!syscall_validate_user_ptr(buf, count))
            return SYS_ERR_FAULT;

        if (fd == 1 || fd == 2) {
            /* Standard output / error to kernel console */
            for (size_t i = 0; i < count; i++) {
                kprintf("%c", buf[i]);
            }
            return (int64_t)count;
        }

        process_t *p = process_get_current();
        if (p && fd >= 0 && fd < (int)MAX_FILE_DESCRIPTORS && p->fd_table[fd]) {
            int64_t written = vfs_write(p->fd_table[fd], buf, count);
            return (written >= 0) ? written : SYS_ERR_BADF;
        }

        return SYS_ERR_BADF;
    }

    case SYS_READ: {
        int fd = (int)arg1;
        void *buf = (void *)(uintptr_t)arg2;
        size_t count = (size_t)arg3;

        if (!syscall_validate_user_ptr(buf, count))
            return SYS_ERR_FAULT;

        if (fd == 0) {
            return 0; /* EOF for default standard input */
        }

        process_t *p = process_get_current();
        if (p && fd >= 0 && fd < (int)MAX_FILE_DESCRIPTORS && p->fd_table[fd]) {
            int64_t read_bytes = vfs_read(p->fd_table[fd], buf, count);
            return (read_bytes >= 0) ? read_bytes : SYS_ERR_BADF;
        }

        return SYS_ERR_BADF;
    }

    case SYS_OPEN: {
        const char *path = (const char *)(uintptr_t)arg1;
        int flags = (int)arg2;

        /* Validate path string up to VFS_MAX_PATH */
        if (!syscall_validate_user_ptr(path, 1))
            return SYS_ERR_FAULT;

        process_t *proc = process_get_current();
        if (!proc)
            return SYS_ERR_PERM;

        /* ZERO Security: Check sandbox path jail */
        uint32_t req_perms = CAP_PERM_READ;
        if ((flags & O_WRONLY) || (flags & O_RDWR)) {
            req_perms |= CAP_PERM_WRITE;
        }
        if (!sandbox_check_path(proc->pid, path, req_perms)) {
            audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, proc->pid, path, false, "sandbox jail access denied");
            return SYS_ERR_PERM;
        }

        /* ZERO Security: Check DAC permissions */
        uint32_t req_mode = PERM_MODE_IROTH;
        if ((flags & O_WRONLY) || (flags & O_RDWR)) {
            req_mode |= PERM_MODE_IWOTH;
        }
        if (perm_check(path, 0, 0, req_mode) != SEC_OK) {
            audit_log(AUDIT_EVENT_PERM_DENY, proc->pid, path, false, "DAC permission check failed");
            return SYS_ERR_PERM;
        }

        /* Find free file descriptor slot */
        int free_fd = -1;
        for (int i = 3; i < (int)MAX_FILE_DESCRIPTORS; i++) {
            if (proc->fd_table[i] == NULL) {
                free_fd = i;
                break;
            }
        }

        if (free_fd == -1)
            return SYS_ERR_NOMEM;

        file_t *f = vfs_open(path, flags);
        if (!f)
            return SYS_ERR_BADF;

        proc->fd_table[free_fd] = f;
        return (int64_t)free_fd;
    }

    case SYS_CLOSE: {
        int fd = (int)arg1;
        process_t *p = process_get_current();
        if (!p)
            return SYS_ERR_PERM;

        if (fd < 0 || fd >= (int)MAX_FILE_DESCRIPTORS || !p->fd_table[fd])
            return SYS_ERR_BADF;

        vfs_close(p->fd_table[fd]);
        p->fd_table[fd] = NULL;
        return SYS_SUCCESS;
    }

    case SYS_IPC_SEND: {
        pid_t target_pid = (pid_t)arg1;
        const ipc_message_t *msg = (const ipc_message_t *)(uintptr_t)arg2;

        if (!syscall_validate_user_ptr(msg, sizeof(ipc_message_t)))
            return SYS_ERR_FAULT;

        process_t *proc = process_get_current();
        if (proc) {
            const sandbox_policy_t *pol = sandbox_get_policy(proc->pid);
            if (pol && !pol->ipc_allowed) {
                audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, proc->pid, "ipc:send", false, "IPC forbidden by sandbox");
                return SYS_ERR_PERM;
            }
        }

        int res = ipc_send(target_pid, msg);
        if (res == 0) return SYS_SUCCESS;
        if (res == -2) return SYS_ERR_NOMEM;
        return SYS_ERR_INVAL;
    }

    case SYS_IPC_RECV: {
        ipc_message_t *out_msg = (ipc_message_t *)(uintptr_t)arg1;
        bool non_blocking = (bool)arg2;

        if (!syscall_validate_user_ptr(out_msg, sizeof(ipc_message_t)))
            return SYS_ERR_FAULT;

        int res = ipc_recv(out_msg, non_blocking);
        return (res == 0) ? SYS_SUCCESS : SYS_ERR_INVAL;
    }

    case SYS_SOCKET: {
        process_t *proc = process_get_current();
        if (proc && !sandbox_check_network(proc->pid)) {
            audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, proc->pid, "net:socket", false, "network socket disallowed by sandbox");
            return SYS_ERR_PERM;
        }
        int domain = (int)arg1;
        int type = (int)arg2;
        int protocol = (int)arg3;
        int sock = sys_sock_socket(domain, type, protocol);
        return (sock >= 0) ? (int64_t)sock : SYS_ERR_INVAL;
    }

    case SYS_BIND: {
        int fd = (int)arg1;
        uint32_t ip = (uint32_t)arg2;
        uint16_t port = (uint16_t)arg3;
        int res = sys_sock_bind(fd, ip, port);
        return (res == 0) ? SYS_SUCCESS : SYS_ERR_INVAL;
    }

    case SYS_LISTEN: {
        int fd = (int)arg1;
        int backlog = (int)arg2;
        int res = sys_sock_listen(fd, backlog);
        return (res == 0) ? SYS_SUCCESS : SYS_ERR_INVAL;
    }

    case SYS_ACCEPT: {
        int fd = (int)arg1;
        uint32_t rem_ip = 0;
        uint16_t rem_port = 0;
        int client_fd = sys_sock_accept(fd, &rem_ip, &rem_port);
        return (client_fd >= 0) ? (int64_t)client_fd : SYS_ERR_INVAL;
    }

    case SYS_CONNECT: {
        int fd = (int)arg1;
        uint32_t ip = (uint32_t)arg2;
        uint16_t port = (uint16_t)arg3;
        int res = sys_sock_connect(fd, ip, port);
        return (res == 0) ? SYS_SUCCESS : SYS_ERR_INVAL;
    }

    case SYS_SEND: {
        int fd = (int)arg1;
        const void *buf = (const void *)(uintptr_t)arg2;
        size_t len = (size_t)arg3;
        if (!syscall_validate_user_ptr(buf, len))
            return SYS_ERR_FAULT;
        return sys_sock_send(fd, buf, len);
    }

    case SYS_RECV: {
        int fd = (int)arg1;
        void *buf = (void *)(uintptr_t)arg2;
        size_t len = (size_t)arg3;
        if (!syscall_validate_user_ptr(buf, len))
            return SYS_ERR_FAULT;
        return sys_sock_recv(fd, buf, len);
    }

    default:
        KLOG_WARN("syscall", "unknown system call %lu requested", num);
        return SYS_ERR_NOSYS;
    }
}
