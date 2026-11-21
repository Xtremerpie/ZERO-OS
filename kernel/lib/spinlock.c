/* ZERO KERNEL - Spinlock Implementation (Version 1.0.0 Core Foundation). */

#include "zero/spinlock.h"
#include "zero/cpu.h"
#include "zero/panic.h"

#define SPINLOCK_TIMEOUT_LOOPS 100000000ull

void spinlock_init(spinlock_t *lock, const char *name)
{
    if (!lock)
        return;
    lock->locked = 0;
    lock->name = name ? name : "unnamed_lock";
    lock->owner_cpu = 0;
    lock->recursion_count = 0;
}

bool spinlock_try_acquire(spinlock_t *lock)
{
    if (!lock)
        return false;
    return __atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE) == 0;
}

void spinlock_acquire(spinlock_t *lock)
{
    if (!lock)
        return;

    uint64_t loops = 0;
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        cpu_pause();
        loops++;
        if (loops >= SPINLOCK_TIMEOUT_LOOPS) {
            kpanic("spinlock: DEADLOCK TIMEOUT on lock '%s' (holding CPU: %u)",
                   lock->name, lock->owner_cpu);
        }
    }
}

void spinlock_release(spinlock_t *lock)
{
    if (!lock)
        return;

    if (!lock->locked) {
        kpanic("spinlock: attempt to release unacquired lock '%s'", lock->name);
    }

    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

void spin_lock_irqsave(spinlock_t *lock, uint64_t *flags)
{
    uint64_t f = read_rflags();
    cli();
    if (flags)
        *flags = f;
    spinlock_acquire(lock);
}

void spin_unlock_irqrestore(spinlock_t *lock, uint64_t flags)
{
    spinlock_release(lock);
    write_rflags(flags);
}
