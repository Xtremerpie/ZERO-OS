/* ZERO KERNEL - ZERO AI Assistant Daemon (Milestone 8.0.0 ZERO Intelligence).
 *
 * Implements the native background AI assistant daemon managing request dispatch,
 * session context tracking, inference orchestration, and health telemetry.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define AI_MAX_SESSIONS      8u
#define AI_MAX_PROMPT_LEN    512u
#define AI_MAX_RESPONSE_LEN  1024u
#define AI_MAX_LOG_ENTRIES   32u

typedef enum {
    AI_DAEMON_IDLE = 0,
    AI_DAEMON_BUSY_INFERENCE,
    AI_DAEMON_AWAITING_APPROVAL,
    AI_DAEMON_EXECUTING_ACTION,
    AI_DAEMON_STOPPED
} ai_daemon_state_t;

typedef struct ai_daemon_log {
    uint64_t timestamp_ms;
    char     message[80];
    bool     is_error;
} ai_daemon_log_t;

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

/* Initialize daemon structures and state */
void ai_daemon_init(void);

/* Daemon lifecycle controls */
bool ai_daemon_start(void);
bool ai_daemon_stop(void);
bool ai_daemon_restart(void);

/* Query daemon status and live logs */
void ai_daemon_get_status(ai_daemon_status_t *out_status);
uint32_t ai_daemon_get_logs(ai_daemon_log_t *out_logs, uint32_t max_logs);
void ai_daemon_log_event(const char *msg, bool is_error);

/* Submit inference request into daemon queue */
int32_t ai_daemon_submit_prompt(uint32_t session_id, const char *prompt, char *out_response, size_t max_resp_len);

/* Cancel active session inference */
bool ai_daemon_cancel(uint32_t session_id);
