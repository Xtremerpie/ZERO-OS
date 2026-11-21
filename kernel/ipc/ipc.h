/* ZERO KERNEL - Inter-Process Communication (IPC) Interface (Version 2.0.0 System Architecture).
 *
 * Defines message formats, ring-buffered mailboxes, and synchronous/asynchronous
 * IPC delivery between isolated processes.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "process/process.h"

#define IPC_MAX_PAYLOAD   128u
#define IPC_QUEUE_CAP     16u

typedef struct ipc_message {
    pid_t sender_pid;
    pid_t target_pid;
    uint32_t type;
    uint32_t length;
    uint8_t payload[IPC_MAX_PAYLOAD];
} ipc_message_t;

/* Initialize the kernel IPC subsystem. */
void ipc_init(void);

/* Send a message to the target process identified by target_pid.
 * Returns 0 on success, negative error code on failure. */
int ipc_send(pid_t target_pid, const ipc_message_t *msg);

/* Receive the next message from the calling process's mailbox.
 * If non_blocking is false and queue is empty, yields or waits.
 * Returns 0 on success, negative error code if empty/error. */
int ipc_recv(ipc_message_t *out_msg, bool non_blocking);

/* Clean up mailbox and discard pending messages for a terminated process. */
void ipc_cleanup_process(pid_t pid);
