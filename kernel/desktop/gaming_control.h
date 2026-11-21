/* ZERO KERNEL - ZERO Gaming Control Center (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements a graphical dashboard for profile switching, real-time FPS/latency
 * monitoring, graphics acceleration status, audio controls, and AI resource coordination.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

/* Initialize gaming control center structures */
void gaming_control_init(void);

/* Launch gaming control center application window */
window_t *gaming_control_launch(void);

/* Check if gaming control center is currently open */
bool gaming_control_is_open(void);
