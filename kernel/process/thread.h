/* ZERO KERNEL - Thread Management Subsystem (Version 2.0.0 System Architecture).
 *
 * Defines the Thread Control Block (TCB), thread lifecycle,
 * execution context, kernel stacks, and user stacks.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "process/process.h"

#define MAX_THREADS       128u
#define KERNEL_STACK_SIZE 16384u /* 16 KiB per kernel thread stack */
#define DEFAULT_TIME_SLICE 5u    /* 5 timer ticks = 50 ms at 100 Hz */

typedef int32_t tid_t;

enum thread_state {
    THREAD_STATE_UNUSED = 0,
    THREAD_STATE_NEW,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_SLEEPING,
    THREAD_STATE_TERMINATED
};

/* Callee-saved x86_64 register context for cooperative/preemptive context switching */
struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t rdi;
    uint64_t rsi;
};

typedef struct thread {
    tid_t tid;
    process_t *process;
    enum thread_state state;
    char name[32];

    /* Kernel execution stack */
    void *kstack_base;
    void *kstack_top;

    /* User stack (for Ring 3 execution) */
    uint64_t user_rsp;
    uint64_t user_rip;

    /* Hardware Context */
    struct cpu_context context;

    /* Scheduling parameters */
    uint32_t priority;
    uint32_t time_slice;
    uint64_t sleep_deadline;

    /* Linked list pointers */
    struct thread *next_in_process;
    struct thread *next_ready;
} thread_t;

/* Initialize thread manager. */
void thread_init(void);

/* Create a kernel thread within a process. */
thread_t *thread_create_kernel(process_t *proc, const char *name, void (*entry_fn)(void *), void *arg);

/* User-mode thread creation is intentionally not exposed until the x86_64
 * privilege-transition path (TSS/IST + iretq frame) is implemented and tested. */

/* Mark thread as terminated and trigger scheduling of next thread. */
void thread_exit(void);

/* Sleep current thread until current_ticks reaches deadline. */
void thread_sleep_ticks(uint64_t ticks);

/* Find thread by TID. */
thread_t *thread_get(tid_t tid);

/* Retrieve currently running thread on the CPU. */
thread_t *thread_get_current(void);

/* Set the currently running thread. */
void thread_set_current(thread_t *t);

/* Reclaim resources of a terminated thread. */
void thread_destroy(thread_t *t);
