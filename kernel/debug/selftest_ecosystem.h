/* ZERO KERNEL - Ecosystem Subsystem Self-Test Interface (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements in-kernel validation for .zapp package parsing, traversal defense,
 * ZPM package manager lifecycle, configuration system, store catalog, and desktop integration.
 */
#pragma once

#include <stdbool.h>

/* Execute ecosystem subsystem self-tests */
bool ecosystem_run_selftests(void);

/* Generate structured ecosystem diagnostic report */
void ecosystem_report_generate(void);
