/* ZERO KERNEL - Process Management Implementation (Version 2.0.0 System Architecture). */

#include "process/process.h"
#include "process/thread.h"
#include "fs/vfs.h"
#include "ipc/ipc.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"
#include "zero/spinlock.h"

static process_t processes[MAX_PROCESSES];
static process_t *current_process = NULL;
static pid_t next_pid = 1;
static spinlock_t proc_lock = SPINLOCK_INIT("proc_lock");

void process_init(void)
{
    memset(processes, 0, sizeof(processes));

    /* Initialize Kernel Process (PID 0) */
    process_t *kproc = &processes[0];
    kproc->pid = 0;
    kproc->ppid = 0;
    strncpy(kproc->name, "kernel_core", MAX_PROCESS_NAME - 1);
    kproc->state = PROCESS_STATE_RUNNING;
    kproc->pml4_phys = vmm_get_kernel_pml4_phys();
    kproc->pml4_virt = vmm_get_kernel_pml4_virt();
    kproc->thread_count = 1;

    current_process = kproc;
    KLOG_INFO("proc", "process manager initialized; kernel process PID 0 active");
}

process_t *process_create(const char *name, pid_t parent_pid)
{
    uint64_t flags;
    spin_lock_irqsave(&proc_lock, &flags);

    process_t *proc = NULL;
    for (size_t i = 1; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_STATE_UNUSED) {
            proc = &processes[i];
            break;
        }
    }

    if (!proc) {
        spin_unlock_irqrestore(&proc_lock, flags);
        KLOG_ERROR("proc", "cannot create process: process table full");
        return NULL;
    }

    /* Allocate private virtual address space */
    uint64_t pml4_phys = 0;
    uint64_t *pml4_virt = NULL;
    enum vmm_status s = vmm_create_address_space(&pml4_phys, &pml4_virt);
    if (s != VMM_OK) {
        spin_unlock_irqrestore(&proc_lock, flags);
        KLOG_ERROR("proc", "failed to allocate address space for process '%s'", name);
        return NULL;
    }

    memset(proc, 0, sizeof(process_t));
    proc->pid = next_pid++;
    proc->ppid = parent_pid;
    if (name)
        strncpy(proc->name, name, MAX_PROCESS_NAME - 1);
    else
        strncpy(proc->name, "unnamed", MAX_PROCESS_NAME - 1);

    proc->state = PROCESS_STATE_NEW;
    proc->pml4_phys = pml4_phys;
    proc->pml4_virt = pml4_virt;
    proc->thread_count = 0;

    spin_unlock_irqrestore(&proc_lock, flags);
    KLOG_INFO("proc", "created process '%s' (PID: %d, PPID: %d, PML4: 0x%016lx)",
              proc->name, proc->pid, proc->ppid, proc->pml4_phys);

    return proc;
}

process_t *process_get(pid_t pid)
{
    if (pid < 0 || (size_t)pid >= MAX_PROCESSES)
        return NULL;

    process_t *p = &processes[pid];
    return (p->state != PROCESS_STATE_UNUSED) ? p : NULL;
}

process_t *process_get_current(void)
{
    return current_process;
}

void process_set_current(process_t *proc)
{
    current_process = proc;
}

void process_terminate(process_t *proc, int exit_code)
{
    if (!proc || proc->pid == 0)
        return;

    uint64_t flags;
    spin_lock_irqsave(&proc_lock, &flags);

    proc->state = PROCESS_STATE_TERMINATED;
    proc->exit_code = exit_code;

    KLOG_INFO("proc", "process '%s' (PID: %d) terminated with exit code %d",
              proc->name, proc->pid, exit_code);

    spin_unlock_irqrestore(&proc_lock, flags);
}

void process_cleanup(process_t *proc)
{
    if (!proc || proc->pid == 0)
        return;

    uint64_t flags;
    spin_lock_irqsave(&proc_lock, &flags);

    /* 1. Close open file descriptors */
    for (size_t i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (proc->fd_table[i]) {
            vfs_close(proc->fd_table[i]);
            proc->fd_table[i] = NULL;
        }
    }

    /* 1b. Clean up IPC mailbox */
    ipc_cleanup_process(proc->pid);

    /* 2. Destroy private address space tables */
    if (proc->pml4_phys && proc->pml4_phys != vmm_get_kernel_pml4_phys()) {
        vmm_destroy_address_space(proc->pml4_phys);
        proc->pml4_phys = 0;
        proc->pml4_virt = NULL;
    }

    /* 3. Mark slot free */
    proc->state = PROCESS_STATE_UNUSED;
    proc->pid = -1;

    spin_unlock_irqrestore(&proc_lock, flags);
}

uint32_t process_count(void)
{
    uint32_t count = 0;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROCESS_STATE_UNUSED)
            count++;
    }
    return count;
}
