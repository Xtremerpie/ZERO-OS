/* ZERO KERNEL - Desktop Shell Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Coordinates desktop lifecycle, application launching, taskbar interaction,
 * global keyboard shortcuts (Alt+Tab, Alt+F4, Win/Super), and workspace state.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"

typedef enum {
    SHELL_STATE_LOGIN = 0,
    SHELL_STATE_DESKTOP,
    SHELL_STATE_LOCKED
} shell_state_t;

/* Built-in Application IDs */
typedef enum {
    APP_ID_TERMINAL = 1,
    APP_ID_FILE_MANAGER,
    APP_ID_CONTROL_CENTER,
    APP_ID_SYSTEM_MONITOR,
    APP_ID_ABOUT,
    APP_ID_STORE,
    APP_ID_CALC,
    APP_ID_EDITOR,
    APP_ID_VIEWER,
    APP_ID_ARCHIVER,
    APP_ID_NETMON,
    APP_ID_SECURITY_CENTER,
    APP_ID_AI_ASSISTANT,
    APP_ID_GAMING_CONTROL,
    APP_ID_DEMO_GAME
} app_id_t;

/* Initialize desktop shell and subsystems */
void shell_init(framebuffer_t *fb);

/* Get and set shell operational state */
shell_state_t shell_get_state(void);
void shell_set_state(shell_state_t state);

/* Dispatch mouse and keyboard events through shell */
void shell_dispatch_mouse(int32_t screen_x, int32_t screen_y, uint8_t buttons);
void shell_dispatch_key(uint8_t key, uint8_t scancode);

/* Launch standard system utility application */
void shell_launch_app(app_id_t app_id);

/* Cycle active window focus (Alt+Tab) */
void shell_cycle_window(void);

/* Periodic shell update / frame composition */
void shell_tick(void);
