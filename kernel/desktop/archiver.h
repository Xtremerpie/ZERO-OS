/* ZERO KERNEL - ZERO Archive Viewer Application (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements native graphical .zapp package archive inspector and extractor.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

/* Initialize ZERO Archive Viewer subsystem */
void archiver_init(void);

/* Launch or focus ZERO Archive Viewer */
window_t *archiver_launch(void);

/* Inspect specific package file */
window_t *archiver_inspect_file(const char *path);

/* Close or query state */
void archiver_close(void);
bool archiver_is_open(void);
