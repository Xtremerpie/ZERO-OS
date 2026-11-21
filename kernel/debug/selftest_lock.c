/* ZERO KERNEL - Spinlock & Concurrency Self-Tests (Version 1.0.0 Core Foundation).
 *
 * Verifies:
 * 1. Lock initialization and zeroing.
 * 2. Acquire and release cycle.
 * 3. Non-blocking try_acquire behavior (returns true when unowned, false when owned).
 * 4. IRQ-safe locking: interrupts are masked during critical section and state restored upon exit.
 */

#include "zero/spinlock.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "lib/spinlock_test.h"

static bool test_basic_lock(void)
{
    spinlock_t lock;
    spinlock_init(&lock, "test_basic");

    if (lock.locked != 0)
        return false;

    spinlock_acquire(&lock);
    if (lock.locked == 0)
        return false;

    /* try_acquire must return false while lock is held */
    if (spinlock_try_acquire(&lock))
        return false;

    spinlock_release(&lock);
    if (lock.locked != 0)
        return false;

    /* try_acquire must return true when lock is released */
    if (!spinlock_try_acquire(&lock))
        return false;

    spinlock_release(&lock);
    return true;
}

static bool test_irq_save_restore(void)
{
    spinlock_t lock;
    spinlock_init(&lock, "test_irq");

    uint64_t initial_rflags = read_rflags();

    uint64_t flags = 0;
    spin_lock_irqsave(&lock, &flags);

    /* Verify interrupts are disabled in CPU (IF flag bit 9 is 0) */
    uint64_t inside_rflags = read_rflags();
    if (inside_rflags & (1ull << 9)) {
        spin_unlock_irqrestore(&lock, flags);
        KLOG_ERROR("lock_test", "interrupts NOT disabled during spin_lock_irqsave");
        return false;
    }

    spin_unlock_irqrestore(&lock, flags);

    /* Verify RFLAGS IF bit matches initial state */
    uint64_t restored_rflags = read_rflags();
    if ((restored_rflags & (1ull << 9)) != (initial_rflags & (1ull << 9))) {
        KLOG_ERROR("lock_test", "interrupt state not preserved after spin_unlock_irqrestore");
        return false;
    }

    return true;
}

bool spinlock_run_selftests(void)
{
    KLOG_INFO("lock_test", "running Spinlock & Concurrency self-test suite...");

    if (!test_basic_lock()) {
        KLOG_ERROR("lock_test", "[FAIL] basic spinlock acquire/release");
        return false;
    }
    kprintf("  [PASS] lock: atomic acquire, try_acquire and release\n");

    if (!test_irq_save_restore()) {
        KLOG_ERROR("lock_test", "[FAIL] IRQ-save lock state preservation");
        return false;
    }
    kprintf("  [PASS] lock: IRQ-safe critical section state preservation\n");

    KLOG_INFO("lock_test", "all Spinlock self-tests passed successfully");
    return true;
}
