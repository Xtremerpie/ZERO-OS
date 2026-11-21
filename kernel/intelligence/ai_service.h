/* ZERO KERNEL - Central AI Intelligence Service (Milestone 8.0.0 ZERO Intelligence).
 *
 * Coordinates the background AI daemon, model management, provider abstraction,
 * permission-gated system automation, memory persistence, and security audit integration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "security/security.h"

typedef enum {
    AI_SERVICE_STOPPED = 0,
    AI_SERVICE_STARTING,
    AI_SERVICE_RUNNING,
    AI_SERVICE_PAUSED,
    AI_SERVICE_ERROR
} ai_service_state_t;

typedef struct ai_service_stats {
    uint64_t total_requests;
    uint64_t total_tokens_generated;
    uint64_t total_actions_proposed;
    uint64_t total_actions_approved;
    uint64_t total_actions_rejected;
    uint32_t active_sessions;
    uint32_t available_models;
    bool     daemon_healthy;
} ai_service_stats_t;

/* Initialize central AI Intelligence service subsystem */
void ai_service_init(void);

/* Query and control service operational state */
ai_service_state_t ai_service_get_state(void);
void ai_service_set_state(ai_service_state_t state);

/* Query service telemetry and operational metrics */
void ai_service_get_stats(ai_service_stats_t *out_stats);

/* Service control interface: start, stop, restart */
bool ai_service_start(void);
bool ai_service_stop(void);
bool ai_service_restart(void);

/* Resource coordination with ZERO Gaming Mode */
void ai_service_set_gaming_mode(bool active);
bool ai_service_is_gaming_mode(void);
