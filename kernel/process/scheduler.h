/* ZERO KERNEL - Preemptive Scheduler (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements a round-robin preemptive scheduler with:
 * - Ready queue and sleeping/blocked queue.
 * - Dedicated idle thread.
 * - Timer-driven preemption via APIC periodic ticks.
 * - Automatic process address space switching (CR3 update).
 * - Dynamic time slice and gaming low-latency profile support.
 * - Foreground process prioritization without background starvation.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "process/thread.h"
#include "process/process.h"

typedef enum {
    SCHED_PROFILE_NORMAL = 0,       /* Balanced: 5 ticks (50ms) per slice */
    SCHED_PROFILE_LOW_LATENCY,      /* Gaming: 8 ticks (80ms) foreground, 2 ticks (20ms) background */
    SCHED_PROFILE_POWER_SAVER       /* Efficiency: 10 ticks (100ms) to coalesce wakeups */
} sched_profile_t;

struct scheduler_stats {
    uint64_t        context_switches;
    uint64_t        preemptions;
    uint64_t        voluntary_yields;
    uint64_t        idle_ticks;
    uint32_t        ready_threads_count;
    sched_profile_t active_profile;
    pid_t           foreground_game_pid;
    uint64_t        max_latency_ticks;
    uint64_t        avg_latency_ticks;
    uint64_t        foreground_boosts;
};

/* Initialize the scheduler and spawn the system idle thread. */
void scheduler_init(void);

/* Check if the scheduler is initialized and active. */
bool scheduler_is_active(void);

/* Add a ready thread to the scheduler's run queue. */
void scheduler_add_thread(thread_t *thread);

/* Voluntarily yield the remaining time slice to the next ready thread. */
void scheduler_yield(void);

/* Invoked by the APIC timer ISR tick to manage preemption and sleeping threads. */
void scheduler_tick(void);

/* Core scheduling logic: picks next ready thread and executes context switch. */
void scheduler_schedule(void);

/* Retrieve scheduler performance statistics. */
void scheduler_get_stats(struct scheduler_stats *out_stats);

/* Configure dynamic scheduler performance profile */
void scheduler_set_profile(sched_profile_t profile);
sched_profile_t scheduler_get_profile(void);

/* Set/get foreground game process for latency-critical prioritization */
void scheduler_set_foreground_game(pid_t pid);
pid_t scheduler_get_foreground_game(void);
