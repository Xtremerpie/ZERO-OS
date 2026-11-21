/* ZERO KERNEL - AI Audit & Security Telemetry (Milestone 8.0.0 ZERO Intelligence).
 *
 * Connects AI assistant decisions, model invocations, and automation approvals
 * directly with the central ZERO Security audit logger.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "intelligence/ai_automation.h"

/* Record AI assistant session lifecycle events */
void ai_audit_session_event(uint32_t session_id, uint32_t pid, bool created, const char *title);

/* Record model load/unload events */
void ai_audit_model_event(uint32_t model_id, const char *model_name, bool loaded, uint32_t ram_mb);

/* Record provider switch events */
void ai_audit_provider_event(uint32_t provider_id, const char *prov_name, bool activated);

/* Record automation proposal, approval, execution, or denial */
void ai_audit_action_event(const ai_action_t *action, const char *details);
