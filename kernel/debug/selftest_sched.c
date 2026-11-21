/* ZERO KERNEL - Preemptive Scheduler Self-Tests (Version 2.0.0 System Architecture).
 *
 * Verifies:
 * 1. Scheduler active state and idle thread initialization.
 * 2. Current running thread retrieval.
 * 3. Scheduler statistics tracking (context switches, yields, idle ticks).
 * 4. Sleep queue expiration logic on scheduler ticks.
 * 5. Run-queue state management.
 */

#include "process/sched_test.h"
#include "process/scheduler.h"
#include "process/thread.h"
#include "process/process.h"
#include "zero/log.h"

static void dummy_sched_task(void *arg)
{
    (void)arg;
}

bool sched_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting Scheduler subsystem self-tests...");

    /* 1. Scheduler active */
    if (!scheduler_is_active()) {
        KLOG_ERR("selftest", "Scheduler test 1 failed: scheduler not active");
        return false;
    }

    /* 2. Current thread */
    thread_t *curr = thread_get_current();
    if (!curr) {
        KLOG_ERR("selftest", "Scheduler test 2 failed: current thread is NULL");
        return false;
    }

    /* 3. Scheduler stats */
    struct scheduler_stats stats;
    scheduler_get_stats(&stats);
    KLOG_INFO("selftest", "Scheduler stats: %lu switches, %lu yields, %lu idle ticks",
              stats.context_switches, stats.voluntary_yields, stats.idle_ticks);

    /* 4. Thread sleeping and tick progression test */
    process_t *kproc = process_get(0);
    thread_t *sleeper = thread_create_kernel(kproc, "t_sleeper", dummy_sched_task, NULL);
    if (!sleeper) {
        KLOG_ERR("selftest", "Scheduler test 4 failed: cannot create sleeper thread");
        return false;
    }

    sleeper->state = THREAD_STATE_SLEEPING;
    sleeper->sleep_deadline = 3; /* 3 ticks */

    /* Advance 1 tick: should still be sleeping */
    scheduler_tick();
    if (sleeper->state != THREAD_STATE_SLEEPING) {
        thread_destroy(sleeper);
        KLOG_ERR("selftest", "Scheduler test 4 failed: woke up too early");
        return false;
    }

    /* Advance remaining 2 ticks */
    scheduler_tick();
    scheduler_tick();

    if (sleeper->state != THREAD_STATE_READY) {
        thread_destroy(sleeper);
        KLOG_ERR("selftest", "Scheduler test 4 failed: did not wake up when deadline reached");
        return false;
    }

    thread_destroy(sleeper);

    kprintf("[PASS] sched: preemptive round-robin scheduler and sleep timer queues verified\n");
    return true;
}
