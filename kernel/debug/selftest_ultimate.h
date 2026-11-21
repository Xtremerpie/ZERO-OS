/* ZERO KERNEL - ZERO Ultimate Self-Test Interface (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements in-kernel validation for gaming mode profiles, low-latency scheduler,
 * graphics hardware abstraction, audio device subsystem, and AI resource coordination.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Execute all ZERO Ultimate subsystem self-tests */
bool ultimate_run_selftests(void);

/* Generate structured ultimate release diagnostic report */
void ultimate_report_generate(void);

#ifdef __cplusplus
}
#endif
