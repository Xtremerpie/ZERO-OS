/* ZERO KERNEL - Inter-Process Communication (IPC) Implementation (Version 2.0.0 System Architecture). */

#include "ipc/ipc.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "zero/string.h"
#include "zero/spinlock.h"
#include "zero/log.h"

typedef struct ipc_mailbox {
    spinlock_t lock;
    ipc_message_t queue[IPC_QUEUE_CAP];
    size_t head;
    size_t tail;
    size_t count;
} ipc_mailbox_t;

static ipc_mailbox_t g_mailboxes[MAX_PROCESSES];

void ipc_init(void)
{
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        spinlock_init(&g_mailboxes[i].lock);
        g_mailboxes[i].head = 0;
        g_mailboxes[i].tail = 0;
        g_mailboxes[i].count = 0;
        memset(g_mailboxes[i].queue, 0, sizeof(g_mailboxes[i].queue));
    }
    KLOG_INFO("ipc", "IPC subsystem initialized (mailboxes: %u, queue cap: %u)",
              (unsigned)MAX_PROCESSES, (unsigned)IPC_QUEUE_CAP);
}

int ipc_send(pid_t target_pid, const ipc_message_t *msg)
{
    if (target_pid < 0 || (uint32_t)target_pid >= MAX_PROCESSES || !msg)
        return -1;

    if (msg->length > IPC_MAX_PAYLOAD)
        return -1;

    process_t *target_proc = process_get(target_pid);
    if (!target_proc || target_proc->state == PROCESS_STATE_TERMINATED || target_proc->state == PROCESS_STATE_UNUSED)
        return -1;

    process_t *curr = process_get_current();
    pid_t sender_pid = curr ? curr->pid : 0;

    ipc_mailbox_t *mb = &g_mailboxes[target_pid];
    spinlock_acquire(&mb->lock);

    if (mb->count >= IPC_QUEUE_CAP) {
        spinlock_release(&mb->lock);
        return -2; /* Mailbox full */
    }

    ipc_message_t *slot = &mb->queue[mb->tail];
    memcpy(slot, msg, sizeof(ipc_message_t));
    slot->sender_pid = sender_pid;
    slot->target_pid = target_pid;

    mb->tail = (mb->tail + 1) % IPC_QUEUE_CAP;
    mb->count++;

    spinlock_release(&mb->lock);
    return 0;
}

int ipc_recv(ipc_message_t *out_msg, bool non_blocking)
{
    if (!out_msg)
        return -1;

    process_t *curr = process_get_current();
    pid_t my_pid = curr ? curr->pid : 0;
    if (my_pid < 0 || (uint32_t)my_pid >= MAX_PROCESSES)
        return -1;

    ipc_mailbox_t *mb = &g_mailboxes[my_pid];

    for (int retry = 0; retry < 5; retry++) {
        spinlock_acquire(&mb->lock);

        if (mb->count > 0) {
            memcpy(out_msg, &mb->queue[mb->head], sizeof(ipc_message_t));
            mb->head = (mb->head + 1) % IPC_QUEUE_CAP;
            mb->count--;
            spinlock_release(&mb->lock);
            return 0;
        }

        spinlock_release(&mb->lock);

        if (non_blocking) {
            return -1; /* No messages available */
        }

        scheduler_yield();
    }

    return -1;
}

void ipc_cleanup_process(pid_t pid)
{
    if (pid < 0 || (uint32_t)pid >= MAX_PROCESSES)
        return;

    ipc_mailbox_t *mb = &g_mailboxes[pid];
    spinlock_acquire(&mb->lock);
    mb->head = 0;
    mb->tail = 0;
    mb->count = 0;
    memset(mb->queue, 0, sizeof(mb->queue));
    spinlock_release(&mb->lock);
}
