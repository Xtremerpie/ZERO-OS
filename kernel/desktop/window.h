/* ZERO KERNEL - Window Management Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements full window lifecycle, z-order stacking, focus management,
 * titlebar controls, hit-testing, dragging, maximize/restore, minimize, and event dispatch.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"
#include "graphics/color.h"

#define MAX_WINDOWS              32u
#define WINDOW_TITLEBAR_HEIGHT   26
#define WINDOW_BORDER_WIDTH      1
#define WINDOW_MIN_WIDTH         140
#define WINDOW_MIN_HEIGHT        80

#define WINDOW_BTN_WIDTH         18
#define WINDOW_BTN_HEIGHT        18

/* Window State Flags */
#define WINDOW_FLAG_VISIBLE      (1u << 0)
#define WINDOW_FLAG_FOCUSED      (1u << 1)
#define WINDOW_FLAG_MINIMIZED    (1u << 2)
#define WINDOW_FLAG_MAXIMIZED    (1u << 3)
#define WINDOW_FLAG_DRAGGING     (1u << 4)
#define WINDOW_FLAG_RESIZABLE    (1u << 5)
#define WINDOW_FLAG_CLOSING      (1u << 6)
#define WINDOW_FLAG_ALWAYS_TOP   (1u << 7)

/* Window Hit-Test Regions */
typedef enum {
    WM_HIT_NONE = 0,
    WM_HIT_TITLEBAR,
    WM_HIT_CLOSE,
    WM_HIT_MAXIMIZE,
    WM_HIT_MINIMIZE,
    WM_HIT_CLIENT,
    WM_HIT_BORDER
} wm_hit_region_t;

struct window;

typedef void (*win_paint_cb_t)(struct window *win, framebuffer_t *fb);
typedef void (*win_mouse_cb_t)(struct window *win, int32_t local_x, int32_t local_y, uint8_t buttons);
typedef void (*win_key_cb_t)(struct window *win, uint8_t key, uint8_t scancode);
typedef void (*win_close_cb_t)(struct window *win);

typedef struct window {
    int32_t         id;
    char            title[64];
    int32_t         x;
    int32_t         y;
    int32_t         width;
    int32_t         height;

    /* Saved geometry for maximize / restore */
    int32_t         saved_x;
    int32_t         saved_y;
    int32_t         saved_width;
    int32_t         saved_height;

    uint32_t        flags;
    int32_t         z_order;

    /* Client area dimensions and dedicated off-screen buffer */
    int32_t         client_width;
    int32_t         client_height;
    uint32_t       *content;

    /* Drag tracking state */
    int32_t         drag_offset_x;
    int32_t         drag_offset_y;

    /* Event callbacks */
    win_paint_cb_t  on_paint;
    win_mouse_cb_t  on_mouse;
    win_key_cb_t    on_key;
    win_close_cb_t  on_close;
    void           *user_data;
} window_t;

/* Initialize Window Manager with desktop bounds */
void wm_init(int32_t screen_w, int32_t screen_h);

/* Create and register a new window */
window_t *wm_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags);

/* Destroy window and free all allocated resources */
void wm_destroy_window(window_t *win);

/* Focus and bring window to topmost z-order */
void wm_bring_to_front(window_t *win);
void wm_set_focus(window_t *win);
window_t *wm_get_focused(void);

/* Window geometry and state operations */
void wm_move(window_t *win, int32_t new_x, int32_t new_y);
void wm_resize(window_t *win, int32_t new_w, int32_t new_h);
void wm_minimize(window_t *win);
void wm_maximize(window_t *win);
void wm_restore(window_t *win);
void wm_close(window_t *win);

/* Query and hit-testing */
window_t *wm_get_at(int32_t x, int32_t y);
wm_hit_region_t wm_hit_test(window_t *win, int32_t x, int32_t y);

/* Input event dispatch to windows */
void wm_handle_mouse(int32_t screen_x, int32_t screen_y, uint8_t buttons);
void wm_handle_key(uint8_t key, uint8_t scancode);

/* Window enumeration (returns array of windows and count) */
window_t **wm_get_windows(int32_t *count);
int32_t wm_get_window_count(void);
window_t *wm_get_by_id(int32_t id);
