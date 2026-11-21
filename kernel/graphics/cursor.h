/* ZERO KERNEL - Mouse Cursor & Input Integration Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements hardware-integrated mouse pointer rendering with background save/restore,
 * button state tracking, position clamping, and desktop event forwarding.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"

#define CURSOR_WIDTH   16u
#define CURSOR_HEIGHT  16u

#define MOUSE_BTN_LEFT    (1u << 0)
#define MOUSE_BTN_RIGHT   (1u << 1)
#define MOUSE_BTN_MIDDLE  (1u << 2)

typedef struct mouse_cursor {
    int32_t  x;
    int32_t  y;
    int32_t  prev_x;
    int32_t  prev_y;
    uint8_t  buttons;
    uint8_t  prev_buttons;
    bool     visible;

    /* Background preservation buffer to prevent cursor flicker */
    uint32_t saved_bg[CURSOR_WIDTH * CURSOR_HEIGHT];
    bool     has_saved_bg;
    int32_t  saved_x;
    int32_t  saved_y;
} mouse_cursor_t;

/* Initialize mouse cursor subsystem */
void cursor_init(uint32_t start_x, uint32_t start_y);

/* Update cursor position with relative deltas from mouse hardware */
void cursor_move_delta(int32_t dx, int32_t dy);

/* Set absolute cursor position */
void cursor_set_pos(int32_t x, int32_t y);

/* Get current cursor coordinates and button state */
void cursor_get_state(int32_t *out_x, int32_t *out_y, uint8_t *out_buttons);

/* Erase cursor from previous position by restoring saved background */
void cursor_restore_bg(framebuffer_t *fb);

/* Render cursor at current position, saving background underneath */
void cursor_draw(framebuffer_t *fb);

/* Update button state from hardware */
void cursor_set_buttons(uint8_t buttons);

/* Set cursor visibility */
void cursor_set_visible(bool visible);
