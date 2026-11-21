/* ZERO KERNEL - Process Management Subsystem (Version 2.0.0 System Architecture).
 *
 * Defines the Process Control Block (PCB), process lifecycle states,
 * private address space ownership, file descriptor tables, and resource cleanup.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES      64u
#define MAX_PROCESS_NAME   32u
#define MAX_FILE_DESCRIPTORS 32u

typedef int32_t pid_t;

enum process_state {
    PROCESS_STATE_UNUSED = 0,
    PROCESS_STATE_NEW,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED,
    PROCESS_STATE_ZOMBIE
};

/* Forward declaration for thread control block and file */
struct thread;
struct file;

typedef struct process {
    pid_t pid;
    pid_t ppid;
    char name[MAX_PROCESS_NAME];
    enum process_state state;
    int exit_code;

    /* Private Virtual Memory Address Space */
    uint64_t pml4_phys;
    uint64_t *pml4_virt;

    /* File Descriptor Table */
    struct file *fd_table[MAX_FILE_DESCRIPTORS];

    /* Threads belonging to this process */
    struct thread *main_thread;
    uint32_t thread_count;

    /* Resource accounting */
    uint64_t memory_bytes_allocated;
    uint64_t cpu_time_ticks;
} process_t;

/* Initialize the process manager subsystem. */
void process_init(void);

/* Create a new process with its own private virtual address space. */
process_t *process_create(const char *name, pid_t parent_pid);

/* Terminate a process with an exit status code. */
void process_terminate(process_t *proc, int exit_code);

/* Find a process by its PID. Returns NULL if not found or unused. */
process_t *process_get(pid_t pid);

/* Retrieve the currently executing process. */
process_t *process_get_current(void);

/* Set the currently executing process. */
void process_set_current(process_t *proc);

/* Release all memory, address space tables, and open file descriptors of a terminated process. */
void process_cleanup(process_t *proc);

/* Count active processes in the system. */
uint32_t process_count(void);
