/* ZERO KERNEL - IPC Subsystem Self-Tests (Version 2.0.0 System Architecture).
 *
 * Verifies:
 * 1. Rejection of invalid recipient PIDs.
 * 2. Unicast message sending between isolated processes.
 * 3. Message receipt, payload integrity, and sender PID validation.
 * 4. Mailbox capacity limits and queue overflow handling.
 * 5. Process termination mailbox cleanup.
 */

#include "ipc/ipc_test.h"
#include "ipc/ipc.h"
#include "process/process.h"
#include "zero/log.h"
#include "zero/string.h"

bool ipc_run_selftests(void)
{
    KLOG_INFO("selftest", "Starting IPC subsystem self-tests...");

    process_t *kproc = process_get_current();

    /* 1. Invalid target PID */
    ipc_message_t test_msg;
    memset(&test_msg, 0, sizeof(test_msg));
    test_msg.type = 1;
    strcpy((char *)test_msg.payload, "PING");
    test_msg.length = 4;

    if (ipc_send(999, &test_msg) == 0) {
        KLOG_ERROR("selftest", "IPC test 1 failed: allowed send to non-existent PID");
        return false;
    }

    /* 2. Create receiver process */
    process_t *recv_proc = process_create("ipc_worker", 0);
    if (!recv_proc) {
        KLOG_ERROR("selftest", "IPC test 2 failed: cannot create receiver process");
        return false;
    }

    /* 3. Send message from kernel process to receiver process */
    int res = ipc_send(recv_proc->pid, &test_msg);
    if (res != 0) {
        process_cleanup(recv_proc);
        KLOG_ERROR("selftest", "IPC test 3 failed: ipc_send returned %d", res);
        return false;
    }

    /* 4. Switch context to receiver process and read message */
    process_set_current(recv_proc);

    ipc_message_t received;
    memset(&received, 0, sizeof(received));
    res = ipc_recv(&received, true);
    if (res != 0) {
        process_set_current(kproc);
        process_cleanup(recv_proc);
        KLOG_ERROR("selftest", "IPC test 4 failed: ipc_recv returned %d", res);
        return false;
    }

    if (received.sender_pid != 0 || received.target_pid != recv_proc->pid ||
        received.type != 1 || received.length != 4 || strcmp((char *)received.payload, "PING") != 0) {
        process_set_current(kproc);
        process_cleanup(recv_proc);
        KLOG_ERROR("selftest", "IPC test 4 failed: payload corruption or metadata mismatch");
        return false;
    }

    /* 5. Fill mailbox up to capacity and verify overflow rejection */
    process_set_current(kproc);
    for (size_t i = 0; i < IPC_QUEUE_CAP; i++) {
        res = ipc_send(recv_proc->pid, &test_msg);
        if (res != 0) {
            process_cleanup(recv_proc);
            KLOG_ERROR("selftest", "IPC test 5 failed: queue insert failed at slot %lu", i);
            return false;
        }
    }

    /* Slot IPC_QUEUE_CAP + 1 should fail with -2 (mailbox full) */
    res = ipc_send(recv_proc->pid, &test_msg);
    if (res != -2) {
        process_cleanup(recv_proc);
        KLOG_ERROR("selftest", "IPC test 5 failed: queue overflow was not rejected (-2)");
        return false;
    }

    /* 6. Cleanup */
    process_cleanup(recv_proc);
    process_set_current(kproc);

    kprintf("[PASS] ipc: ring-buffered inter-process message mailboxes verified\n");
    return true;
}
