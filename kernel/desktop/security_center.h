/* ZERO KERNEL - ZERO Security Center Application (Milestone 7.0.0 ZERO Security).
 *
 * Provides a unified graphical interface for inspecting and controlling system security:
 * capability inspection, process sandboxing, firewall rules, package verification,
 * live audit event streaming, and hardware/memory protection status.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

#define SECURITY_CENTER_WIDTH   640
#define SECURITY_CENTER_HEIGHT  440

/* Initialize Security Center subsystem */
void security_center_init(void);

/* Launch or focus Security Center window */
window_t *security_center_launch(void);

/* Check if Security Center window is open */
bool security_center_is_open(void);

/* Request refresh of Security Center view */
void security_center_refresh(void);
