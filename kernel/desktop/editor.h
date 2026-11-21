/* ZERO KERNEL - ZERO Text Editor Application (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements native graphical text editor:
 * multi-line text buffer, keyboard typing, file open and save via VFS.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "desktop/window.h"

/* Initialize ZERO Text Editor subsystem */
void editor_init(void);

/* Launch or focus ZERO Text Editor */
window_t *editor_launch(void);

/* Open specific file in editor */
window_t *editor_open_file(const char *path);

/* Close or query state */
void editor_close(void);
bool editor_is_open(void);
