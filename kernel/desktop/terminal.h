/* ZERO KERNEL - ZERO Terminal Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements interactive terminal console inside a native window:
 * scrollback buffer, command prompt, and execution of system commands:
 * help, clear, version, memory, cpu, devices, storage, network,
 * processes, uptime, ls, cd, pwd, cat.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

#define TERM_MAX_LINES   24u
#define TERM_MAX_COLS    64u

/* Launch or focus ZERO Terminal */
window_t *terminal_launch(void);

/* Close or query state */
void terminal_close(void);
bool terminal_is_open(void);

/* Execute command string in terminal */
void terminal_execute_command(const char *cmd_line);

/* Write text to terminal output buffer */
void terminal_puts(const char *str);
void terminal_clear(void);
