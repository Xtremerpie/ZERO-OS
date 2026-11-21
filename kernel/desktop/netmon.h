/* ZERO KERNEL - Native Network Monitor Application (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Real-time network statistics, interface status, ARP table viewer, and active socket inspector.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

#define NETMON_WIDTH   540
#define NETMON_HEIGHT  360

/* Initialize network monitor subsystem */
void netmon_init(void);

/* Launch network monitor window */
window_t *netmon_launch(void);

/* Check if network monitor window is currently open */
bool netmon_is_open(void);

/* Force refresh of network monitor display */
void netmon_refresh(void);
