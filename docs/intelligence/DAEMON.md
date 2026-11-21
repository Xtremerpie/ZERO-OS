# ZERO Intelligence — Assistant Daemon Specification
**Milestone:** 8.0.0 — ZERO Intelligence  
**Component:** `kernel/intelligence/ai_daemon.h`, `ai_daemon.c`  

---

## 1. Overview

The ZERO AI Assistant Daemon (`ai_daemon`) is a background system service responsible for receiving incoming AI prompt requests, managing conversational worker sessions, dispatching generation tasks to the active provider, and logging operational status.

The daemon operates in an event-driven loop and maintains explicit health metrics, queue depth indicators, and a circular memory trace log.

---

## 2. State Machine

```
                   +------------------------+
                   |   AI_DAEMON_STOPPED    |
                   +-----------+------------+
                               | ai_daemon_start()
                               v
+------------------------> +------------------------+
|                          |     AI_DAEMON_IDLE     |<------------------------+
|                          +-----------+------------+                         |
|                                      | Prompt Submitted                     |
|                                      v                                      |
|                          +------------------------+                         |
|                          |AI_DAEMON_BUSY_INFERENCE|                         |
|                          +-----------+------------+                         |
|                                      |                                      |
|            +-------------------------+-------------------------+            |
|            | No Action Proposed                                | Action     |
|            v                                                   v Proposed   |
|   (Complete Output)                             +--------------------------+|
|            |                                    |AI_DAEMON_AWAITING_APPRVAL||
|            |                                    +--------------+-----------+|
|            |                                                   |            |
|            |                                 +-----------------+------------+
|            |                                 | Approved        | Rejected   |
|            |                                 v                 v            |
|            |                    +------------------------+ (Abort Action)   |
|            |                    |AI_DAEMON_EXECUTING_ACT |                  |
|            |                    +------------+-----------+                  |
|            |                                 | Execution Finished           |
|            +---------------------------------+------------------------------+
```

### State Definitions
- `AI_DAEMON_IDLE`: Daemon is online and listening. Memory footprint is minimal.
- `AI_DAEMON_BUSY_INFERENCE`: A session is actively synthesizing text or evaluating intent via the active provider.
- `AI_DAEMON_AWAITING_APPROVAL`: The inference generated a sensitive automation proposal. The daemon pauses execution for this session and signals the Desktop UI for user consent.
- `AI_DAEMON_EXECUTING_ACTION`: User has approved the action; the broker is executing the requested syscall or VFS operation within a sandbox.
- `AI_DAEMON_STOPPED`: Daemon has been administratively stopped via Control Center or CLI.

---

## 3. Data Structures

### 3.1 Daemon Status & Telemetry (`ai_daemon_status_t`)
```c
typedef struct ai_daemon_status {
    ai_daemon_state_t state;
    uint32_t          active_sessions;
    uint64_t          uptime_ms;
    uint32_t          queue_depth;
    uint32_t          active_model_id;
    char              active_model_name[32];
    uint32_t          memory_used_kb;
    bool              healthy;
} ai_daemon_status_t;
```

### 3.2 Circular Event Log (`ai_daemon_log_t`)
```c
typedef struct ai_daemon_log {
    uint64_t timestamp_ms;
    char     message[80];
    bool     is_error;
} ai_daemon_log_t;
```
The daemon retains the last 32 events in a high-speed lockless circular buffer. When the buffer fills, the oldest entry is automatically evicted.

---

## 4. Lifecycle & Control Functions

| Function | Signature | Description |
|---|---|---|
| `ai_daemon_init` | `void ai_daemon_init(void)` | Zeroes data structures, resets ring buffers, marks state idle. |
| `ai_daemon_start` | `bool ai_daemon_start(void)` | Starts daemon worker loop and marks daemon healthy. |
| `ai_daemon_stop` | `bool ai_daemon_stop(void)` | Flushes in-flight work and places daemon in STOPPED state. |
| `ai_daemon_restart` | `bool ai_daemon_restart(void)` | Atomic stop followed by re-initialization. |
| `ai_daemon_submit_prompt` | `int32_t ai_daemon_submit_prompt(uint32_t session_id, const char *prompt, char *out_response, size_t max_resp_len)` | Submits a prompt synchronously to the inference pipeline. |
| `ai_daemon_get_status` | `void ai_daemon_get_status(ai_daemon_status_t *out_status)` | Returns instantaneous snapshot of daemon health and state. |
| `ai_daemon_get_logs` | `uint32_t ai_daemon_get_logs(ai_daemon_log_t *out_logs, uint32_t max_logs)` | Copies out recent entries from the diagnostic circular buffer. |
