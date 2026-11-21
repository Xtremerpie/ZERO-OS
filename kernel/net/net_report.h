/* ZERO KERNEL - Network Diagnostic Reporting Interface (Milestone 4.0.0 Storage & Networking).
 *
 * Generates human-readable and structured diagnostic reports of network interfaces,
 * traffic statistics, ARP tables, active sockets, and protocol engines.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Generate and display structured ZERO OS network diagnostic report */
void net_report_generate(void);
