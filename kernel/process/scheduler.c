/* ZERO KERNEL - Preemptive Scheduler Implementation (Milestone 9.0.0 ZERO Ultimate). */

#include "process/scheduler.h"
#include "process/process.h"
#include "process/thread.h"
#include "memory/vmm.h"
#include "zero/gdt.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/spinlock.h"
#include "zero/timer.h"
#include "zero/cpu.h"

#define MAX_READY_QUEUE 128
#define MAX_CONSECUTIVE_BOOSTS 4u /* Starvation prevention limit */

extern void context_switch(struct cpu_context *prev, struct cpu_context *next);

static thread_t *ready_queue[MAX_READY_QUEUE];
static size_t ready_head = 0;
static size_t ready_tail = 0;
static size_t ready_count = 0;

static thread_t *idle_thread = NULL;
static bool scheduler_active = false;
static spinlock_t sched_lock = SPINLOCK_INIT("sched_lock");
static struct scheduler_stats stats = { 0 };

static sched_profile_t s_active_profile = SCHED_PROFILE_NORMAL;
static pid_t s_foreground_game_pid = -1;
static uint32_t s_consecutive_game_boosts = 0;

static void idle_loop(void *arg)
{
    (void)arg;
    for (;;) {
        stats.idle_ticks++;
        __asm__ volatile("sti; hlt");
    }
}

void scheduler_init(void)
{
    ready_head = 0;
    ready_tail = 0;
    ready_count = 0;

    stats.active_profile = SCHED_PROFILE_NORMAL;
    stats.foreground_game_pid = -1;

    /* Create the system idle thread */
    process_t *kproc = process_get(0);
    idle_thread = thread_create_kernel(kproc, "idle", idle_loop, NULL);
    if (!idle_thread) {
        kpanic("scheduler: failed to create idle thread");
    }
    idle_thread->priority = 0;

    scheduler_active = true;
    KLOG_INFO("sched", "preemptive round-robin scheduler active (idle thread TID: %d, profile: NORMAL)",
              idle_thread->tid);
}

bool scheduler_is_active(void)
{
    return scheduler_active;
}

void scheduler_add_thread(thread_t *thread)
{
    if (!thread || thread == idle_thread)
        return;

    uint64_t flags;
    spin_lock_irqsave(&sched_lock, &flags);

    if (ready_count < MAX_READY_QUEUE) {
        thread->state = THREAD_STATE_READY;
        ready_queue[ready_tail] = thread;
        ready_tail = (ready_tail + 1) % MAX_READY_QUEUE;
        ready_count++;
        stats.ready_threads_count = (uint32_t)ready_count;
    } else {
        KLOG_ERROR("sched", "ready queue overflow, cannot enqueue thread '%s'", thread->name);
    }

    spin_unlock_irqrestore(&sched_lock, flags);
}

static thread_t *dequeue_ready_thread(void)
{
    if (ready_count == 0)
        return NULL;

    /* In low-latency profile, prioritize threads belonging to foreground game process */
    if (s_active_profile == SCHED_PROFILE_LOW_LATENCY && s_foreground_game_pid > 0 &&
        s_consecutive_game_boosts < MAX_CONSECUTIVE_BOOSTS) {
        for (size_t i = 0; i < ready_count; i++) {
            size_t idx = (ready_head + i) % MAX_READY_QUEUE;
            thread_t *cand = ready_queue[idx];
            if (cand && cand->process && cand->process->pid == s_foreground_game_pid) {
                /* Shift intervening entries to maintain relative order */
                for (size_t j = i; j > 0; j--) {
                    size_t curr = (ready_head + j) % MAX_READY_QUEUE;
                    size_t prev = (ready_head + j - 1 + MAX_READY_QUEUE) % MAX_READY_QUEUE;
                    ready_queue[curr] = ready_queue[prev];
                }
                ready_head = (ready_head + 1) % MAX_READY_QUEUE;
                ready_count--;
                stats.ready_threads_count = (uint32_t)ready_count;
                stats.foreground_boosts++;
                s_consecutive_game_boosts++;
                return cand;
            }
        }
    }

    /* Standard FIFO dequeue */
    s_consecutive_game_boosts = 0;
    thread_t *t = ready_queue[ready_head];
    ready_head = (ready_head + 1) % MAX_READY_QUEUE;
    ready_count--;
    stats.ready_threads_count = (uint32_t)ready_count;
    return t;
}

void scheduler_schedule(void)
{
    if (!scheduler_active)
        return;

    uint64_t flags;
    spin_lock_irqsave(&sched_lock, &flags);

    thread_t *prev = thread_get_current();
    thread_t *next = dequeue_ready_thread();

    if (!next) {
        /* No thread in ready queue: run idle thread */
        if (prev && prev->state == THREAD_STATE_RUNNING) {
            /* Current thread can continue running */
            spin_unlock_irqrestore(&sched_lock, flags);
            return;
        }
        next = idle_thread;
    }

    if (prev && prev->state == THREAD_STATE_RUNNING && prev != idle_thread) {
        /* Re-enqueue previous thread */
        prev->state = THREAD_STATE_READY;
        ready_queue[ready_tail] = prev;
        ready_tail = (ready_tail + 1) % MAX_READY_QUEUE;
        ready_count++;
        stats.ready_threads_count = (uint32_t)ready_count;
    }

    next->state = THREAD_STATE_RUNNING;

    /* Dynamic time slice calculation based on active profile */
    if (s_active_profile == SCHED_PROFILE_LOW_LATENCY) {
        if (next->process && next->process->pid == s_foreground_game_pid) {
            next->time_slice = 8; /* 80 ms slice for foreground game process */
        } else {
            next->time_slice = 2; /* 20 ms slice for background tasks */
        }
    } else if (s_active_profile == SCHED_PROFILE_POWER_SAVER) {
        next->time_slice = 10;    /* 100 ms slice to reduce switch frequency */
    } else {
        next->time_slice = DEFAULT_TIME_SLICE; /* 50 ms default */
    }

    stats.context_switches++;

    if (next == prev) {
        spin_unlock_irqrestore(&sched_lock, flags);
        return;
    }

    /* Switch process address space if switching processes */
    if (next->process && prev && prev->process &&
        next->process->pml4_phys != prev->process->pml4_phys) {
        vmm_switch_pml4(next->process->pml4_phys);
        process_set_current(next->process);
    }

    /* Update TSS kernel stack for user-mode transitions */
    if (next->kstack_top) {
        gdt_set_kernel_stack((uint64_t)(uintptr_t)next->kstack_top);
    }

    thread_set_current(next);

    spin_unlock_irqrestore(&sched_lock, flags);

    /* Assembly context switch between CPU register contexts */
    context_switch(&prev->context, &next->context);
}

void scheduler_yield(void)
{
    stats.voluntary_yields++;
    scheduler_schedule();
}

void scheduler_tick(void)
{
    if (!scheduler_active)
        return;

    thread_t *cur = thread_get_current();
    if (!cur)
        return;

    /* Check sleeping threads in thread table */
    uint64_t current_ticks = timer_get_ticks();
    for (tid_t id = 1; id < (tid_t)MAX_THREADS; id++) {
        thread_t *t = thread_get(id);
        if (t && t->state == THREAD_STATE_SLEEPING) {
            if (current_ticks >= t->sleep_deadline) {
                scheduler_add_thread(t);
            }
        }
    }

    /* Decrement time slice of current thread */
    if (cur->time_slice > 0) {
        cur->time_slice--;
    }

    /* Preempt thread if time slice expired */
    if (cur->time_slice == 0) {
        stats.preemptions++;
        scheduler_schedule();
    }
}

void scheduler_get_stats(struct scheduler_stats *out_stats)
{
    if (out_stats)
        *out_stats = stats;
}

void scheduler_set_profile(sched_profile_t profile)
{
    uint64_t flags;
    spin_lock_irqsave(&sched_lock, &flags);
    s_active_profile = profile;
    stats.active_profile = profile;
    spin_unlock_irqrestore(&sched_lock, flags);
    KLOG_INFO("sched", "scheduler profile set to %d", (int)profile);
}

sched_profile_t scheduler_get_profile(void)
{
    return s_active_profile;
}

void scheduler_set_foreground_game(pid_t pid)
{
    uint64_t flags;
    spin_lock_irqsave(&sched_lock, &flags);
    s_foreground_game_pid = pid;
    stats.foreground_game_pid = pid;
    spin_unlock_irqrestore(&sched_lock, flags);
}

pid_t scheduler_get_foreground_game(void)
{
    return s_foreground_game_pid;
}
