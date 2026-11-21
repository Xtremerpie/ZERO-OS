/* ZERO KERNEL - ZERO System Monitor Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements real-time resource utilization monitor:
 * CPU utilization bars, physical RAM allocation breakdowns,
 * process table, and storage/network throughput counters.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

/* Launch or focus ZERO System Monitor */
window_t *sysmon_launch(void);

/* Close or query state */
void sysmon_close(void);
bool sysmon_is_open(void);
