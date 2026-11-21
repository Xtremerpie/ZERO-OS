/* ZERO KERNEL - Native ZERO AI Assistant Application (Milestone 8.0.0 ZERO Intelligence).
 *
 * Provides a native graphical interface for interacting with the AI Assistant,
 * managing local/remote models, approving permission-gated automations,
 * inspecting memory, configuring providers, and monitoring daemon telemetry.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

#define AI_ASSISTANT_WIDTH   640
#define AI_ASSISTANT_HEIGHT  440

/* Initialize AI Assistant window subsystem */
void ai_assistant_init(void);

/* Launch or focus AI Assistant window */
window_t *ai_assistant_launch(void);

/* Check if AI Assistant window is open */
bool ai_assistant_is_open(void);

/* Request refresh of AI Assistant view */
void ai_assistant_refresh(void);
