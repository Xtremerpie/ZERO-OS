/* ZERO KERNEL - ZERO AI Assistant Daemon Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_daemon.h"
#include "intelligence/ai_engine.h"
#include "intelligence/ai_model.h"
#include "intelligence/ai_audit.h"
#include "zero/timer.h"
#include "zero/string.h"
#include <stdio.h>

static ai_daemon_state_t s_daemon_state = AI_DAEMON_STOPPED;
static uint64_t          s_start_time_ms = 0;
static ai_daemon_log_t   s_logs[AI_MAX_LOG_ENTRIES];
static uint32_t          s_log_head = 0;
static uint32_t          s_log_count = 0;

void ai_daemon_log_event(const char *msg, bool is_error)
{
    if (!msg) return;
    ai_daemon_log_t *l = &s_logs[s_log_head];
    l->timestamp_ms = timer_is_initialized() ? timer_get_uptime_ms() : 0;
    strncpy(l->message, msg, sizeof(l->message) - 1);
    l->message[sizeof(l->message) - 1] = '\0';
    l->is_error = is_error;

    s_log_head = (s_log_head + 1) % AI_MAX_LOG_ENTRIES;
    if (s_log_count < AI_MAX_LOG_ENTRIES) s_log_count++;
}

void ai_daemon_init(void)
{
    s_daemon_state = AI_DAEMON_STOPPED;
    s_start_time_ms = 0;
    memset(s_logs, 0, sizeof(s_logs));
    s_log_head = 0;
    s_log_count = 0;

    ai_daemon_start();
}

bool ai_daemon_start(void)
{
    if (s_daemon_state != AI_DAEMON_STOPPED) return true;

    s_daemon_state = AI_DAEMON_IDLE;
    s_start_time_ms = timer_is_initialized() ? timer_get_uptime_ms() : 0;
    ai_daemon_log_event("ZERO AI Assistant Daemon started", false);
    return true;
}

bool ai_daemon_stop(void)
{
    if (s_daemon_state == AI_DAEMON_STOPPED) return true;

    s_daemon_state = AI_DAEMON_STOPPED;
    ai_daemon_log_event("ZERO AI Assistant Daemon stopped", false);
    return true;
}

bool ai_daemon_restart(void)
{
    ai_daemon_stop();
    return ai_daemon_start();
}

void ai_daemon_get_status(ai_daemon_status_t *out_status)
{
    if (!out_status) return;
    memset(out_status, 0, sizeof(*out_status));

    out_status->state = s_daemon_state;
    out_status->healthy = (s_daemon_state != AI_DAEMON_STOPPED);
    out_status->uptime_ms = (s_daemon_state != AI_DAEMON_STOPPED && timer_is_initialized())
                            ? (timer_get_uptime_ms() - s_start_time_ms) : 0;

    ai_session_t sessions[AI_MAX_SESSIONS];
    out_status->active_sessions = ai_engine_list_sessions(sessions, AI_MAX_SESSIONS);

    const ai_model_desc_t *m = ai_model_get_active();
    if (m) {
        out_status->active_model_id = m->id;
        strncpy(out_status->active_model_name, m->name, sizeof(out_status->active_model_name) - 1);
        out_status->memory_used_kb = m->ram_required_mb * 1024;
    } else {
        strcpy(out_status->active_model_name, "None");
        out_status->memory_used_kb = 0;
    }
}

uint32_t ai_daemon_get_logs(ai_daemon_log_t *out_logs, uint32_t max_logs)
{
    if (!out_logs || max_logs == 0) return 0;
    uint32_t available = (s_log_count < max_logs) ? s_log_count : max_logs;
    for (uint32_t i = 0; i < available; i++) {
        uint32_t idx = (s_log_head + AI_MAX_LOG_ENTRIES - 1 - i) % AI_MAX_LOG_ENTRIES;
        out_logs[i] = s_logs[idx];
    }
    return available;
}

int32_t ai_daemon_submit_prompt(uint32_t session_id, const char *prompt, char *out_response, size_t max_resp_len)
{
    if (s_daemon_state == AI_DAEMON_STOPPED) {
        ai_daemon_log_event("Request rejected: Daemon stopped", true);
        return -1;
    }

    s_daemon_state = AI_DAEMON_BUSY_INFERENCE;
    char log_buf[80];
    snprintf(log_buf, sizeof(log_buf), "Processing prompt: %.40s...", prompt ? prompt : "");
    ai_daemon_log_event(log_buf, false);

    ai_engine_result_t res;
    bool ok = ai_engine_process_prompt(session_id, prompt, &res);

    if (ok) {
        if (out_response && max_resp_len > 0) {
            strncpy(out_response, res.response_text, max_resp_len - 1);
            out_response[max_resp_len - 1] = '\0';
        }
        s_daemon_state = res.requires_approval ? AI_DAEMON_AWAITING_APPROVAL : AI_DAEMON_IDLE;
        return (int32_t)strlen(res.response_text);
    }

    s_daemon_state = AI_DAEMON_IDLE;
    ai_daemon_log_event("Prompt processing failed", true);
    return -2;
}

bool ai_daemon_cancel(uint32_t session_id)
{
    (void)session_id;
    if (s_daemon_state == AI_DAEMON_BUSY_INFERENCE) {
        s_daemon_state = AI_DAEMON_IDLE;
        ai_daemon_log_event("Inference request cancelled", false);
        return true;
    }
    return false;
}
