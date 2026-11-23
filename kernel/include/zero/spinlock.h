/* ZERO KERNEL - Spinlock & IRQ-Safe Concurrency Primitives (Version 1.0.0 Core Foundation).
 *
 * Implements atomic test-and-set spinlocks with:
 * - CPU pause instruction in contention loops.
 * - IRQ-safe critical sections (spin_lock_irqsave / spin_unlock_irqrestore).
 * - Deadlock timeout diagnostics.
 * - Non-blocking try-lock support.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct spinlock {
    volatile uint32_t locked;
    const char *name;
    uint32_t owner_cpu;
    uint32_t recursion_count;
} spinlock_t;

#define SPINLOCK_INIT(lock_name) { .locked = 0, .name = lock_name, .owner_cpu = 0, .recursion_count = 0 }

/* Initialize a spinlock. */
void spinlock_init(spinlock_t *lock, const char *name);

/* Acquire spinlock, busy-waiting with cpu_pause(). */
void spinlock_acquire(spinlock_t *lock);

/* Attempt to acquire spinlock without blocking. Returns true if acquired, false otherwise. */
bool spinlock_try_acquire(spinlock_t *lock);

/* Release spinlock. */
void spinlock_release(spinlock_t *lock);

/* Acquire spinlock while safely disabling interrupts; saves prior RFLAGS in *flags. */
void spin_lock_irqsave(spinlock_t *lock, uint64_t *flags);

/* Release spinlock and restore prior interrupt state from flags. */
void spin_unlock_irqrestore(spinlock_t *lock, uint64_t flags);

/* Compatibility for older subsystems; new code should use acquire/release. */
static inline void spinlock_init_compat(spinlock_t *lock) { spinlock_init(lock, "legacy"); }
#define ZERO_SPINLOCK_INIT_SELECT(_1, _2, NAME, ...) NAME
#define spinlock_init(...) ZERO_SPINLOCK_INIT_SELECT(__VA_ARGS__, spinlock_init, spinlock_init_compat)(__VA_ARGS__)
#define spin_lock(lock) spinlock_acquire(lock)
#define spin_unlock(lock) spinlock_release(lock)
