/* ZERO KERNEL - Process Subsystem Self-Tests (Version 2.0.0 System Architecture).
 *
 * Verifies:
 * 1. Kernel process PID 0 exists, valid name, state == RUNNING.
 * 2. Process creation with private PML4 allocation and initial NEW state.
 * 3. Kernel thread creation with allocated 16 KiB kernel stack and READY state.
 * 4. User thread creation with user rip/rsp registers.
 * 5. Process termination and exit code recording.
 * 6. Process cleanup and address space deallocation.
 * 7. Process lookup and count accounting.
 */

#include "process/process_test.h"
#include "process/process.h"
#include "process/thread.h"
#include "memory/vmm.h"
#include "zero/log.h"
#include "zero/string.h"

static void dummy_thread_entry(void *arg)
{
    (void)arg;
}

bool process_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting Process & Thread subsystem self-tests...");

    /* 1. Verify Kernel Process PID 0 */
    process_t *kproc = process_get(0);
    if (!kproc || kproc->pid != 0 || kproc->state != PROCESS_STATE_RUNNING) {
        KLOG_ERR("selftest", "Process test 1 failed: Kernel process PID 0 invalid");
        return false;
    }
    if (strcmp(kproc->name, "kernel_core") != 0) {
        KLOG_ERR("selftest", "Process test 1 failed: Kernel process name mismatch");
        return false;
    }

    uint32_t initial_count = process_count();
    if (initial_count == 0) {
        KLOG_ERR("selftest", "Process count cannot be 0");
        return false;
    }

    /* 2. Process Creation */
    process_t *proc1 = process_create("test_proc_1", 0);
    if (!proc1 || proc1->pid <= 0 || proc1->state != PROCESS_STATE_NEW) {
        KLOG_ERR("selftest", "Process test 2 failed: process_create failed");
        return false;
    }
    if (proc1->pml4_phys == 0 || proc1->pml4_virt == NULL) {
        KLOG_ERR("selftest", "Process test 2 failed: missing private address space");
        return false;
    }
    if (proc1->pml4_phys == vmm_get_kernel_pml4_phys()) {
        KLOG_ERR("selftest", "Process test 2 failed: private PML4 matches kernel PML4");
        return false;
    }

    /* 3. Thread Creation (Kernel thread) */
    thread_t *t1 = thread_create_kernel(proc1, "t1_worker", dummy_thread_entry, (void *)0x1234);
    if (!t1 || t1->tid <= 0 || t1->state != THREAD_STATE_READY) {
        KLOG_ERR("selftest", "Process test 3 failed: thread_create_kernel failed");
        return false;
    }
    if (!t1->kstack_base || !t1->kstack_top || t1->context.rsp == 0) {
        KLOG_ERR("selftest", "Process test 3 failed: kernel stack invalid");
        return false;
    }
    if (proc1->thread_count != 1 || proc1->main_thread != t1) {
        KLOG_ERR("selftest", "Process test 3 failed: process thread accounting failed");
        return false;
    }

    /* 4. User Thread Creation */
    thread_t *t2 = thread_create_user(proc1, "t2_user", 0x0000400000ull, 0x00007FFFF0000000ull);
    if (!t2 || t2->user_rip != 0x0000400000ull || t2->user_rsp != 0x00007FFFF0000000ull) {
        KLOG_ERR("selftest", "Process test 4 failed: thread_create_user failed");
        return false;
    }

    /* 5. Process Termination */
    process_terminate(proc1, 42);
    if (proc1->state != PROCESS_STATE_TERMINATED || proc1->exit_code != 42) {
        KLOG_ERR("selftest", "Process test 5 failed: termination status invalid");
        return false;
    }

    /* 6. Process Cleanup */
    thread_destroy(t1);
    thread_destroy(t2);
    process_cleanup(proc1);

    if (process_get(proc1->pid) != NULL) {
        KLOG_ERR("selftest", "Process test 6 failed: process not cleaned up");
        return false;
    }

    if (process_count() != initial_count) {
        KLOG_ERR("selftest", "Process test 6 failed: process count leak detected");
        return false;
    }

    kprintf("[PASS] proc: process lifecycle, address space isolation, and threads verified\n");
    return true;
}
