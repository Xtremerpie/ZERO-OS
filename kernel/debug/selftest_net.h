/* ZERO KERNEL - Network Subsystem Self-Test Interface (Milestone 4.0.0 Storage & Networking). */
#pragma once

#include <stdbool.h>

/* Execute network architecture, packet buffers, protocols, and socket API self-tests */
bool net_run_selftests(void);
