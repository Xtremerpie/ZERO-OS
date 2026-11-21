/* ZERO KERNEL - ZERO Calculator Application (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements native graphical desktop calculator:
 * keypad grid, arithmetic operations (+, -, *, /), clear, and result display.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

/* Initialize ZERO Calculator subsystem */
void calc_init(void);

/* Launch or focus ZERO Calculator */
window_t *calc_launch(void);

/* Close or query state */
void calc_close(void);
bool calc_is_open(void);
