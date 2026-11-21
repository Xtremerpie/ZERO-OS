/* ZERO KERNEL - About ZERO OS Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements native About dialog:
 * system branding, version specifications, kernel architectural details,
 * and project mission statement.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

/* Launch or focus About ZERO OS dialog */
window_t *about_launch(void);

/* Close or query state */
void about_close(void);
bool about_is_open(void);
