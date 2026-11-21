/* ZERO KERNEL - Security Subsystem Self-Test Interface (Milestone 7.0.0 ZERO Security).
 *
 * Implements in-kernel validation for capability authorization, attenuation, revocation,
 * process sandboxing containment, POSIX DAC permissions, FIPS SHA-256 KATs,
 * constant-time comparisons, firewall filtering, and audit logging.
 */
#pragma once

#include <stdbool.h>

/* Execute security subsystem self-tests */
bool security_run_selftests(void);

/* Generate structured security diagnostic report */
void security_report_generate(void);
