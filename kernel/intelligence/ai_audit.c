/* ZERO KERNEL - AI Audit & Security Telemetry Implementation (Milestone 8.0.0 ZERO Intelligence).
 */
#include "intelligence/ai_audit.h"
#include "security/audit.h"
#include "zero/string.h"
#include <stdio.h>

void ai_audit_session_event(uint32_t session_id, uint32_t pid, bool created, const char *title)
{
    char res[48];
    snprintf(res, sizeof(res), "ai.session.%u", session_id);
    char details[64];
    snprintf(details, sizeof(details), "%s: %s", created ? "Session opened" : "Session closed",
             (title && title[0]) ? title : "default");
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, pid, res, created, details);
}

void ai_audit_model_event(uint32_t model_id, const char *model_name, bool loaded, uint32_t ram_mb)
{
    char res[48];
    snprintf(res, sizeof(res), "ai.model.%s", (model_name && model_name[0]) ? model_name : "unknown");
    char details[64];
    snprintf(details, sizeof(details), "Model ID %u %s (est %u MB RAM)",
             model_id, loaded ? "loaded" : "unloaded", ram_mb);
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, res, loaded, details);
}

void ai_audit_provider_event(uint32_t provider_id, const char *prov_name, bool activated)
{
    char res[48];
    snprintf(res, sizeof(res), "ai.provider.%s", (prov_name && prov_name[0]) ? prov_name : "unknown");
    char details[64];
    snprintf(details, sizeof(details), "Provider ID %u %s", provider_id, activated ? "active" : "inactive");
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, res, activated, details);
}

void ai_audit_action_event(const ai_action_t *action, const char *details)
{
    if (!action) return;

    bool granted = (action->status == AI_ACTION_STATUS_APPROVED ||
                    action->status == AI_ACTION_STATUS_EXECUTED);

    audit_event_type_t ev_type = granted ? AUDIT_EVENT_CAP_GRANT : AUDIT_EVENT_PERM_DENY;
    if (action->status == AI_ACTION_STATUS_SECURITY_DENIED) {
        ev_type = AUDIT_EVENT_SANDBOX_VIOLATION;
    }

    audit_log(ev_type, action->caller_pid, action->target, granted,
              (details && details[0]) ? details : action->description);
}
