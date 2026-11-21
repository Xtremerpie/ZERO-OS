/* ZERO KERNEL - ZERO Assistant Core Request Engine Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_engine.h"
#include "intelligence/ai_provider.h"
#include "intelligence/ai_model.h"
#include "intelligence/ai_automation.h"
#include "intelligence/ai_audit.h"
#include "intelligence/ai_memory.h"
#include "zero/timer.h"
#include "zero/string.h"
#include <stdio.h>

static ai_session_t s_sessions[AI_MAX_SESSIONS];
static uint32_t     s_session_seq = 0;

void ai_engine_init(void)
{
    memset(s_sessions, 0, sizeof(s_sessions));
    s_session_seq = 0;
}

uint32_t ai_engine_create_session(uint32_t pid, const char *title)
{
    int32_t slot = -1;
    for (uint32_t i = 0; i < AI_MAX_SESSIONS; i++) {
        if (!s_sessions[i].in_use) {
            slot = (int32_t)i;
            break;
        }
    }
    if (slot < 0) return 0;

    s_session_seq++;
    ai_session_t *s = &s_sessions[slot];
    memset(s, 0, sizeof(*s));
    s->id = s_session_seq;
    s->caller_pid = pid;
    s->created_ms = timer_is_initialized() ? timer_get_uptime_ms() : 0;
    s->last_active_ms = s->created_ms;
    s->in_use = true;

    if (title && title[0]) {
        strncpy(s->title, title, sizeof(s->title) - 1);
    } else {
        snprintf(s->title, sizeof(s->title), "Session #%u", s->id);
    }

    const ai_model_desc_t *m = ai_model_get_active();
    s->active_model_id = m ? m->id : 1;

    /* Add initial system welcome context */
    ai_engine_add_message(s->id, AI_MSG_ROLE_SYSTEM, "ZERO OS Intelligence Assistant active under security boundaries.");

    ai_audit_session_event(s->id, pid, true, s->title);
    return s->id;
}

bool ai_engine_close_session(uint32_t session_id)
{
    ai_session_t *s = ai_engine_get_session(session_id);
    if (!s) return false;

    ai_memory_persist_session(s);
    ai_audit_session_event(s->id, s->caller_pid, false, s->title);
    s->in_use = false;
    return true;
}

ai_session_t *ai_engine_get_session(uint32_t session_id)
{
    for (uint32_t i = 0; i < AI_MAX_SESSIONS; i++) {
        if (s_sessions[i].in_use && s_sessions[i].id == session_id) {
            return &s_sessions[i];
        }
    }
    return NULL;
}

uint32_t ai_engine_list_sessions(ai_session_t *out_sessions, uint32_t max_sessions)
{
    if (!out_sessions || max_sessions == 0) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < AI_MAX_SESSIONS && count < max_sessions; i++) {
        if (s_sessions[i].in_use) {
            out_sessions[count++] = s_sessions[i];
        }
    }
    return count;
}

bool ai_engine_add_message(uint32_t session_id, ai_msg_role_t role, const char *text)
{
    ai_session_t *s = ai_engine_get_session(session_id);
    if (!s || !text) return false;

    if (s->msg_count >= AI_MAX_SESSION_MSGS) {
        /* Bounded context: shift older messages to preserve memory */
        for (uint32_t i = 1; i < AI_MAX_SESSION_MSGS; i++) {
            s->messages[i - 1] = s->messages[i];
        }
        s->msg_count = AI_MAX_SESSION_MSGS - 1;
    }

    ai_message_t *m = &s->messages[s->msg_count++];
    m->role = role;
    m->timestamp_ms = timer_is_initialized() ? timer_get_uptime_ms() : 0;
    strncpy(m->text, text, sizeof(m->text) - 1);
    m->text[sizeof(m->text) - 1] = '\0';

    s->last_active_ms = m->timestamp_ms;
    return true;
}

bool ai_engine_clear_session(uint32_t session_id)
{
    ai_session_t *s = ai_engine_get_session(session_id);
    if (!s) return false;

    s->msg_count = 0;
    ai_engine_add_message(s->id, AI_MSG_ROLE_SYSTEM, "Context cleared.");
    return true;
}

bool ai_engine_process_prompt(uint32_t session_id, const char *user_input, ai_engine_result_t *out_res)
{
    if (!user_input || !out_res) return false;
    memset(out_res, 0, sizeof(*out_res));

    ai_session_t *s = ai_engine_get_session(session_id);
    if (!s) {
        /* Auto-create default session if none exists */
        session_id = ai_engine_create_session(0, "Default Session");
        s = ai_engine_get_session(session_id);
        if (!s) return false;
    }

    /* Record user message */
    ai_engine_add_message(session_id, AI_MSG_ROLE_USER, user_input);

    /* Action Intent Classification */
    ai_action_type_t act_type = AI_ACTION_NONE;
    char target[AI_ACTION_TARGET_LEN] = "";
    char desc[AI_ACTION_DESC_LEN] = "";

    if (strncmp(user_input, "open ", 5) == 0 || strncmp(user_input, "launch ", 7) == 0) {
        act_type = AI_ACTION_APP_LAUNCH;
        const char *arg = (user_input[0] == 'o') ? (user_input + 5) : (user_input + 7);
        strncpy(target, arg, sizeof(target) - 1);
        snprintf(desc, sizeof(desc), "Launch application '%s'", target);
    } else if (strncmp(user_input, "read ", 5) == 0) {
        act_type = AI_ACTION_FILE_READ;
        strncpy(target, user_input + 5, sizeof(target) - 1);
        snprintf(desc, sizeof(desc), "Read authorized file '%s'", target);
    } else if (strncmp(user_input, "write ", 6) == 0) {
        act_type = AI_ACTION_FILE_WRITE;
        strncpy(target, user_input + 6, sizeof(target) - 1);
        snprintf(desc, sizeof(desc), "Create/modify file '%s'", target);
    } else if (strncmp(user_input, "notify ", 7) == 0) {
        act_type = AI_ACTION_NOTIFICATION;
        strncpy(target, user_input + 7, sizeof(target) - 1);
        snprintf(desc, sizeof(desc), "Post desktop notification '%s'", target);
    } else if (strstr(user_input, "sysinfo") || strstr(user_input, "system status")) {
        act_type = AI_ACTION_SYS_INFO;
        strcpy(target, "kernel.metrics");
        strcpy(desc, "Read system uptime and security status");
    }

    if (act_type != AI_ACTION_NONE) {
        /* Propose automation action through permission broker */
        uint32_t act_id = ai_automation_propose(session_id, s->caller_pid, act_type, target, desc);
        const ai_action_t *action = ai_automation_get_by_id(act_id);

        out_res->has_action_proposal = true;
        out_res->proposed_action_id = act_id;
        out_res->action_type = act_type;
        strncpy(out_res->action_target, target, sizeof(out_res->action_target) - 1);
        out_res->requires_approval = action ? action->requires_user_approval : false;
        out_res->success = (act_id != 0);

        if (out_res->requires_approval) {
            snprintf(out_res->response_text, sizeof(out_res->response_text),
                     "I propose action: %s. This sensitive operation requires your explicit confirmation.", desc);
        } else {
            /* Auto-execute safe action */
            char exec_res[256];
            ai_automation_execute(act_id, exec_res, sizeof(exec_res));
            snprintf(out_res->response_text, sizeof(out_res->response_text),
                     "Executed: %s -> %s", desc, exec_res);
        }
    } else {
        /* Standard conversational inference via active provider */
        int32_t gen_rc = ai_provider_generate(user_input, out_res->response_text, sizeof(out_res->response_text));
        out_res->has_action_proposal = false;
        out_res->success = (gen_rc >= 0);
    }

    /* Record assistant response */
    ai_engine_add_message(session_id, AI_MSG_ROLE_ASSISTANT, out_res->response_text);
    ai_memory_persist_session(s);

    return true;
}
