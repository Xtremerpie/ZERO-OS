/* ZERO KERNEL - Graphical Login & Authentication Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements the official ZERO OS graphical login screen with branded visual card,
 * credentials entry (username/password), development authentication mode with explicit
 * security notice, and seamless transition to the desktop shell.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"

typedef enum {
    LOGIN_STATUS_PENDING = 0,
    LOGIN_STATUS_SUCCESS,
    LOGIN_STATUS_FAILED
} login_status_t;

/* Initialize login subsystem */
void login_init(framebuffer_t *fb);

/* Render complete login surface */
void login_render(framebuffer_t *fb);

/* Handle input events targeted at login screen */
void login_handle_mouse(int32_t x, int32_t y, uint8_t buttons);
void login_handle_key(uint8_t key, uint8_t scancode);

/* Check if session is authenticated */
bool login_is_authenticated(void);

/* Lock session and return to login screen */
void login_lock(void);

/* Get current authenticated username */
const char *login_get_username(void);
