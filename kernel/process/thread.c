/* ZERO KERNEL - Thread Management Implementation (Version 2.0.0 System Architecture). */

#include "process/thread.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "memory/heap.h"
#include "zero/log.h"
#include "zero/panic.h"
#include "zero/string.h"
#include "zero/spinlock.h"
#include "zero/timer.h"

static thread_t threads[MAX_THREADS];
static thread_t *current_thread = NULL;
static tid_t next_tid = 1;
static spinlock_t thread_lock = SPINLOCK_INIT("thread_lock");

/* Trampoline function invoked when a new kernel thread begins execution */
static void thread_trampoline(void (*entry_fn)(void *), void *arg)
{
    if (entry_fn) {
        entry_fn(arg);
    }
    thread_exit();
}

void thread_init(void)
{
    memset(threads, 0, sizeof(threads));

    /* Initialize Thread 0 (Kernel Main Thread) */
    thread_t *kthread = &threads[0];
    kthread->tid = 0;
    kthread->process = process_get(0);
    strncpy(kthread->name, "kernel_main", 31);
    kthread->state = THREAD_STATE_RUNNING;
    kthread->priority = 10;
    kthread->time_slice = DEFAULT_TIME_SLICE;

    current_thread = kthread;
    if (kthread->process) {
        kthread->process->main_thread = kthread;
        kthread->process->thread_count = 1;
    }

    KLOG_INFO("thread", "thread manager initialized; kernel thread TID 0 active");
}

thread_t *thread_create_kernel(process_t *proc, const char *name, void (*entry_fn)(void *), void *arg)
{
    if (!proc || !entry_fn)
        return NULL;

    uint64_t flags;
    spin_lock_irqsave(&thread_lock, &flags);

    thread_t *t = NULL;
    for (size_t i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_STATE_UNUSED) {
            t = &threads[i];
            break;
        }
    }

    if (!t) {
        spin_unlock_irqrestore(&thread_lock, flags);
        KLOG_ERROR("thread", "thread table full");
        return NULL;
    }

    /* Allocate kernel stack */
    void *kstack = kmalloc(KERNEL_STACK_SIZE);
    if (!kstack) {
        spin_unlock_irqrestore(&thread_lock, flags);
        KLOG_ERROR("thread", "failed to allocate kernel stack for thread '%s'", name);
        return NULL;
    }

    memset(t, 0, sizeof(thread_t));
    t->tid = next_tid++;
    t->process = proc;
    if (name)
        strncpy(t->name, name, 31);
    else
        strncpy(t->name, "kthread", 31);

    t->state = THREAD_STATE_READY;
    t->kstack_base = kstack;
    /* Align stack top to 16 bytes, leaving space for return address */
    uint64_t top = (uint64_t)(uintptr_t)kstack + KERNEL_STACK_SIZE;
    top = (top & ~0xFull) - 8;
    t->kstack_top = (void *)(uintptr_t)top;

    /* Setup initial register context for context_switch */
    t->context.rsp = (uint64_t)(uintptr_t)t->kstack_top;
    t->context.rip = (uint64_t)(uintptr_t)thread_trampoline;
    t->context.rdi = (uint64_t)(uintptr_t)entry_fn; /* Arg 0 */
    t->context.rsi = (uint64_t)(uintptr_t)arg;      /* Arg 1 */
    t->context.rflags = 0x202;                      /* Interrupts enabled flag bit */

    t->priority = 10;
    t->time_slice = DEFAULT_TIME_SLICE;

    /* Link into process thread list */
    proc->thread_count++;

    spin_unlock_irqrestore(&thread_lock, flags);

    /* Enqueue to scheduler run queue */
    scheduler_add_thread(t);

    KLOG_INFO("thread", "created kernel thread '%s' (TID: %d, PID: %d, Stack: %p)",
              t->name, t->tid, proc->pid, t->kstack_base);

    return t;
}

thread_t *thread_get(tid_t tid)
{
    if (tid < 0 || (size_t)tid >= MAX_THREADS)
        return NULL;

    thread_t *t = &threads[tid];
    return (t->state != THREAD_STATE_UNUSED) ? t : NULL;
}

thread_t *thread_get_current(void)
{
    return current_thread;
}

void thread_set_current(thread_t *t)
{
    current_thread = t;
}

void thread_exit(void)
{
    thread_t *cur = current_thread;
    if (!cur || cur->tid == 0) {
        kpanic("thread: attempt to exit thread 0 or NULL thread");
    }

    uint64_t flags;
    spin_lock_irqsave(&thread_lock, &flags);
    cur->state = THREAD_STATE_TERMINATED;
    spin_unlock_irqrestore(&thread_lock, flags);

    KLOG_INFO("thread", "thread '%s' (TID: %d) exited", cur->name, cur->tid);

    /* Switch to next runnable thread */
    scheduler_yield();
}

void thread_sleep_ticks(uint64_t ticks)
{
    thread_t *cur = current_thread;
    if (!cur)
        return;

    uint64_t flags;
    spin_lock_irqsave(&thread_lock, &flags);
    cur->state = THREAD_STATE_SLEEPING;
    cur->sleep_deadline = timer_get_ticks() + ticks;
    spin_unlock_irqrestore(&thread_lock, flags);

    scheduler_yield();
}

void thread_destroy(thread_t *t)
{
    if (!t || t->tid == 0)
        return;

    uint64_t flags;
    spin_lock_irqsave(&thread_lock, &flags);

    if (t->kstack_base) {
        kfree(t->kstack_base);
        t->kstack_base = NULL;
        t->kstack_top = NULL;
    }

    t->state = THREAD_STATE_UNUSED;
    t->tid = -1;

    spin_unlock_irqrestore(&thread_lock, flags);
}
