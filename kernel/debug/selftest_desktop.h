/* ZERO KERNEL - Desktop Subsystem Self-Test Interface (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements in-kernel validation for graphics, font engine, cursor overlay,
 * window manager, compositor, shell, login, and desktop applications.
 */
#pragma once

#include <stdbool.h>

/* Execute desktop subsystem self-tests */
bool desktop_run_selftests(void);

/* Generate structured desktop diagnostic report */
void desktop_report_generate(void);
