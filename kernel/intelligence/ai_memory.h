/* ZERO KERNEL - AI Persistent Memory & History Subsystem (Milestone 8.0.0 ZERO Intelligence).
 *
 * Implements structured persistent storage for conversation history, user preferences,
 * project context, and operational action outcomes in VFS (/data/ai/).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "intelligence/ai_engine.h"

#define AI_MEMORY_DIR           "/data/ai"
#define AI_MEMORY_PREFS_FILE    "/data/ai/preferences.cfg"

typedef struct ai_user_prefs {
    char     default_model[32];
    uint32_t max_context_tokens;
    bool     auto_approve_safe_info;
    bool     confirm_sensitive_writes;
    bool     notifications_enabled;
    uint32_t active_provider_id;
} ai_user_prefs_t;

/* Initialize memory subsystem and ensure /data/ai exists */
void ai_memory_init(void);

/* Load and save user assistant preferences */
bool ai_memory_load_prefs(ai_user_prefs_t *out_prefs);
bool ai_memory_save_prefs(const ai_user_prefs_t *prefs);

/* Persist session messages to /data/ai/history */
bool ai_memory_persist_session(const ai_session_t *session);

/* Clear conversation history */
bool ai_memory_clear_all_history(void);
