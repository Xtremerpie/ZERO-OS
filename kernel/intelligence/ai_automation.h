/* ZERO KERNEL - Permission-Gated AI Automation Broker (Milestone 8.0.0 ZERO Intelligence).
 *
 * Enforces strict security boundaries between AI assistant decisions and operating system
 * execution. Bridges AI action proposals with ZERO Security 7.0.0 capabilities,
 * sandbox containment, user approval gates, and structured audit logs.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "security/security.h"
#include "security/capability.h"

#define AI_MAX_PENDING_ACTIONS 16u
#define AI_ACTION_DESC_LEN     64u
#define AI_ACTION_TARGET_LEN   64u

typedef enum {
    AI_ACTION_NONE = 0,
    AI_ACTION_SYS_INFO,         /* Safe read: system stats, uptime, memory */
    AI_ACTION_NOTIFICATION,     /* Display toast notification */
    AI_ACTION_APP_LAUNCH,       /* Launch approved desktop application */
    AI_ACTION_FILE_READ,        /* Read explicitly permitted file */
    AI_ACTION_FILE_WRITE,       /* Write/create permitted file (requires approval) */
    AI_ACTION_SERVICE_CONTROL   /* Start/stop permitted service (requires approval) */
} ai_action_type_t;

typedef enum {
    AI_ACTION_STATUS_NONE = 0,
    AI_ACTION_STATUS_PENDING_APPROVAL,
    AI_ACTION_STATUS_APPROVED,
    AI_ACTION_STATUS_REJECTED,
    AI_ACTION_STATUS_EXECUTED,
    AI_ACTION_STATUS_FAILED,
    AI_ACTION_STATUS_SECURITY_DENIED
} ai_action_status_t;

typedef struct ai_action {
    uint32_t           id;
    uint32_t           session_id;
    uint32_t           caller_pid;
    ai_action_type_t   type;
    char               target[AI_ACTION_TARGET_LEN];
    char               description[AI_ACTION_DESC_LEN];
    bool               requires_user_approval;
    ai_action_status_t status;
    uint64_t           timestamp_ms;
    sec_result_t       sec_result;
    bool               in_use;
} ai_action_t;

/* Initialize automation broker */
void ai_automation_init(void);

/* Propose an automation action from the AI engine */
uint32_t ai_automation_propose(uint32_t session_id, uint32_t pid, ai_action_type_t type,
                              const char *target, const char *desc);

/* User approval / rejection interface */
bool ai_automation_approve(uint32_t action_id);
bool ai_automation_reject(uint32_t action_id);

/* Execute approved action within security sandbox */
bool ai_automation_execute(uint32_t action_id, char *out_result, size_t max_out);

/* Query pending and historical actions */
uint32_t ai_automation_get_pending(ai_action_t *out_actions, uint32_t max_actions);
uint32_t ai_automation_get_history(ai_action_t *out_actions, uint32_t max_actions);
const ai_action_t *ai_automation_get_by_id(uint32_t action_id);
