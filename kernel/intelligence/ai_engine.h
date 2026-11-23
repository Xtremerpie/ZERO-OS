/* ZERO KERNEL - ZERO Assistant Core Request Engine (Milestone 8.0.0 ZERO Intelligence).
 *
 * Implements the assistant request lifecycle: input validation, context assembly,
 * provider inference dispatch, intent parsing, action classification, and response synthesis.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "intelligence/ai_automation.h"

#define AI_MAX_SESSIONS     8u
#define AI_MAX_SESSION_MSGS 16u
#define AI_MSG_TEXT_LEN     256u

typedef enum {
    AI_MSG_ROLE_SYSTEM = 0,
    AI_MSG_ROLE_USER,
    AI_MSG_ROLE_ASSISTANT
} ai_msg_role_t;

typedef struct ai_message {
    ai_msg_role_t role;
    char          text[AI_MSG_TEXT_LEN];
    uint64_t      timestamp_ms;
} ai_message_t;

typedef struct ai_session {
    uint32_t     id;
    char         title[32];
    uint32_t     caller_pid;
    ai_message_t messages[AI_MAX_SESSION_MSGS];
    uint32_t     msg_count;
    uint32_t     active_model_id;
    uint64_t     created_ms;
    uint64_t     last_active_ms;
    bool         in_use;
} ai_session_t;

typedef struct ai_engine_result {
    char             response_text[512];
    bool             has_action_proposal;
    uint32_t         proposed_action_id;
    ai_action_type_t action_type;
    char             action_target[AI_ACTION_TARGET_LEN];
    bool             requires_approval;
    bool             success;
} ai_engine_result_t;

/* Initialize assistant engine */
void ai_engine_init(void);

/* Session management */
uint32_t ai_engine_create_session(uint32_t pid, const char *title);
bool ai_engine_close_session(uint32_t session_id);
ai_session_t *ai_engine_get_session(uint32_t session_id);
uint32_t ai_engine_list_sessions(ai_session_t *out_sessions, uint32_t max_sessions);

/* Process user prompt within session */
bool ai_engine_process_prompt(uint32_t session_id, const char *user_input, ai_engine_result_t *out_res);

/* Add message directly to session context */
bool ai_engine_add_message(uint32_t session_id, ai_msg_role_t role, const char *text);

/* Clear conversation context for a session */
bool ai_engine_clear_session(uint32_t session_id);
