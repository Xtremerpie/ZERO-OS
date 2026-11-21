/* ZERO KERNEL - ZERO Image Viewer Application (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements native graphical image and test pattern visualizer:
 * bitmap rendering, palette display, and zoom scaling.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

/* Initialize ZERO Image Viewer subsystem */
void viewer_init(void);

/* Launch or focus ZERO Image Viewer */
window_t *viewer_launch(void);

/* Close or query state */
void viewer_close(void);
bool viewer_is_open(void);
