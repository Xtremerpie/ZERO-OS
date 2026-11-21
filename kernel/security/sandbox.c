/* ZERO KERNEL - Process Sandboxing & Isolation Engine Implementation (Milestone 7.0.0 ZERO Security).
 */
#include "security/sandbox.h"
#include "security/capability.h"
#include "security/audit.h"
#include "zero/string.h"

static sandbox_policy_t s_proc_policies[SANDBOX_MAX_PROCS];

static void safe_strcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void sandbox_init(void)
{
    memset(s_proc_policies, 0, sizeof(s_proc_policies));

    /* Assign SYSTEM profile to kernel / init process (PID 0) */
    sandbox_assign_profile(0, SANDBOX_PROFILE_SYSTEM, "/");
}

sec_result_t sandbox_assign_profile(uint32_t pid, sandbox_profile_t profile, const char *root_dir)
{
    if (profile == SANDBOX_PROFILE_NONE) return SEC_ERR_BAD_ARG;

    /* Check if already present */
    sandbox_policy_t *p = NULL;
    for (size_t i = 0; i < SANDBOX_MAX_PROCS; i++) {
        if (s_proc_policies[i].in_use && s_proc_policies[i].pid == pid) {
            p = &s_proc_policies[i];
            break;
        }
    }

    if (!p) {
        /* Allocate free slot */
        for (size_t i = 0; i < SANDBOX_MAX_PROCS; i++) {
            if (!s_proc_policies[i].in_use) {
                p = &s_proc_policies[i];
                break;
            }
        }
    }

    if (!p) return SEC_ERR_NO_MEM;

    p->pid = pid;
    p->profile = profile;
    safe_strcpy(p->root_dir, root_dir ? root_dir : "/", sizeof(p->root_dir));
    p->in_use = true;

    switch (profile) {
    case SANDBOX_PROFILE_SYSTEM:
        p->net_allowed = true;
        p->dev_allowed = true;
        p->ipc_allowed = true;
        p->max_open_files = 128;
        break;
    case SANDBOX_PROFILE_STANDARD:
        p->net_allowed = true;
        p->dev_allowed = false;
        p->ipc_allowed = true;
        p->max_open_files = 32;
        break;
    case SANDBOX_PROFILE_BASIC:
        p->net_allowed = false;
        p->dev_allowed = false;
        p->ipc_allowed = true;
        p->max_open_files = 16;
        break;
    case SANDBOX_PROFILE_RESTRICTED:
    default:
        p->net_allowed = false;
        p->dev_allowed = false;
        p->ipc_allowed = false;
        p->max_open_files = 8;
        break;
    }

    audit_log(AUDIT_EVENT_CONFIG_CHANGE, pid, p->root_dir, true, "Sandbox profile assigned to process");
    return SEC_OK;
}

sandbox_profile_t sandbox_get_profile(uint32_t pid)
{
    const sandbox_policy_t *p = sandbox_get_policy(pid);
    return p ? p->profile : SANDBOX_PROFILE_RESTRICTED;
}

const sandbox_policy_t *sandbox_get_policy(uint32_t pid)
{
    for (size_t i = 0; i < SANDBOX_MAX_PROCS; i++) {
        if (s_proc_policies[i].in_use && s_proc_policies[i].pid == pid) {
            return &s_proc_policies[i];
        }
    }
    return NULL;
}

bool sandbox_check_path(uint32_t pid, const char *path, uint32_t req_perms)
{
    if (!path) return false;

    /* PID 0 system process is unconstrained */
    if (pid == 0) return true;

    const sandbox_policy_t *p = sandbox_get_policy(pid);
    if (!p) {
        audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, path, false, "Denied: unprofiled process VFS access");
        return false;
    }

    if (p->profile == SANDBOX_PROFILE_SYSTEM) return true;

    /* Reject path traversal attempts */
    if (strstr(path, "..")) {
        audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, path, false, "Denied: path traversal attempt inside sandbox");
        return false;
    }

    /* Verify if process holds explicit capability token for resource */
    if (cap_check_resource(pid, CAP_TYPE_FILE, path, req_perms) == SEC_OK ||
        cap_check_resource(pid, CAP_TYPE_DIR, path, req_perms) == SEC_OK) {
        return true;
    }

    /* Check if path is inside designated sandbox root */
    if (p->root_dir[0] != '\0' && strcmp(p->root_dir, "/") != 0) {
        size_t rlen = strlen(p->root_dir);
        if (strncmp(path, p->root_dir, rlen) == 0) {
            return true;
        }
    }

    /* Standard read-only access to system fonts and shared assets */
    if ((req_perms & CAP_PERM_WRITE) == 0) {
        if (strncmp(path, "/etc/", 5) == 0 ||
            strncmp(path, "/assets/", 8) == 0 ||
            strncmp(path, "/data/fonts/", 12) == 0) {
            return true;
        }
    }

    audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, path, false, "Denied: path access outside process sandbox boundary");
    return false;
}

bool sandbox_check_network(uint32_t pid)
{
    if (pid == 0) return true;

    const sandbox_policy_t *p = sandbox_get_policy(pid);
    if (!p || !p->net_allowed) {
        audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, "network.socket", false,
                  "Denied: network access prohibited by process sandbox profile");
        return false;
    }

    /* Verify network capability */
    if (cap_check_resource(pid, CAP_TYPE_NET_SOCKET, "*", CAP_PERM_READ | CAP_PERM_WRITE) != SEC_OK &&
        cap_check_resource(pid, CAP_TYPE_NET_SOCKET, "net.socket.all", CAP_PERM_READ | CAP_PERM_WRITE) != SEC_OK) {
        audit_log(AUDIT_EVENT_PERM_DENY, pid, "network.socket", false,
                  "Denied: process lacks network socket capability");
        return false;
    }

    return true;
}

bool sandbox_check_device(uint32_t pid, const char *dev_name)
{
    if (pid == 0) return true;

    const sandbox_policy_t *p = sandbox_get_policy(pid);
    if (!p || !p->dev_allowed) {
        audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, dev_name ? dev_name : "device", false,
                  "Denied: direct device interaction prohibited by sandbox");
        return false;
    }

    return (cap_check_resource(pid, CAP_TYPE_DEVICE, dev_name, CAP_PERM_READ | CAP_PERM_WRITE) == SEC_OK);
}

bool sandbox_check_syscall(uint32_t pid, uint32_t syscall_num)
{
    if (pid == 0) return true;

    const sandbox_policy_t *p = sandbox_get_policy(pid);
    sandbox_profile_t prof = p ? p->profile : SANDBOX_PROFILE_RESTRICTED;

    switch (prof) {
    case SANDBOX_PROFILE_SYSTEM:
        return true;
    case SANDBOX_PROFILE_STANDARD:
        /* All standard syscalls permitted; socket syscalls checked via sandbox_check_network */
        return true;
    case SANDBOX_PROFILE_BASIC:
        /* Socket syscalls (11-17) blocked under BASIC */
        if (syscall_num >= 11 && syscall_num <= 17) {
            audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, "syscall.socket", false,
                      "Denied: socket syscall blocked under BASIC sandbox profile");
            return false;
        }
        return true;
    case SANDBOX_PROFILE_RESTRICTED:
    default:
        /* Network blocked, fork blocked, only basic read/write/exit permitted */
        if ((syscall_num >= 11 && syscall_num <= 17) || syscall_num == 2 /* SYS_FORK */) {
            audit_log(AUDIT_EVENT_SANDBOX_VIOLATION, pid, "syscall.restricted", false,
                      "Denied: privileged syscall blocked under RESTRICTED sandbox");
            return false;
        }
        return true;
    }
}

bool sandbox_validate_user_ptr(const void *ptr, size_t len, bool is_write)
{
    (void)is_write;
    if (!ptr) return false;

    uintptr_t uaddr = (uintptr_t)ptr;

    /* Check arithmetic overflow */
    if (uaddr + len < uaddr) return false;

    /* User canonical address space boundary: must be strictly below higher-half kernel space */
    /* Higher half begins at 0xFFFF800000000000. User space is <= 0x00007FFFFFFFFFFFu */
    if (uaddr >= 0x0000800000000000u || (uaddr + len) > 0x0000800000000000u) {
        return false;
    }

    /* Reject NULL page access [0 .. 0xFFF] */
    if (uaddr < 0x1000u) {
        return false;
    }

    return true;
}
