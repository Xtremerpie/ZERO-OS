# ZERO OS Native Application Framework & API (Milestone 5.0.0)

**Status:** COMPLETE  
**Header:** `kernel/desktop/app.h`, `kernel/desktop/window.h`

---

## 1. Overview

ZERO OS 5.0.0 provides a lightweight, pure-C application framework allowing native applications to create windows, register event callbacks, render 2D content, and respond to mouse and keyboard events.

---

## 2. Application Registration

Applications register their metadata using `app_desc_t`:

```c
typedef struct app_desc {
    app_id_t     id;
    const char  *name;
    uint8_t      icon_glyph;
    window_t   *(*launch)(void);
    bool        (*is_open)(void);
} app_desc_t;
```

### Registered Application IDs:
- `APP_ID_TERMINAL`: ZERO Terminal
- `APP_ID_FILE_MANAGER`: ZERO Files
- `APP_ID_CONTROL_CENTER`: ZERO Control Center
- `APP_ID_SYSTEM_MONITOR`: ZERO System Monitor
- `APP_ID_ABOUT`: About ZERO OS

---

## 3. Window Creation & Event Hooks

To build a native application, implement a launch function that calls `wm_create_window` and binds event callbacks:

```c
#include "desktop/window.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"

static window_t *s_my_app_win = NULL;

static void my_app_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    /* Background */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* Text */
    font_draw_string(fb, ox + 16, oy + 16, "Hello from ZERO OS Native App!", 1, COLOR_ZERO_GOLD);
}

static void my_app_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    if (buttons & 1u) {
        /* Left mouse button clicked inside client area at (lx, ly) */
    }
}

static void my_app_key(window_t *win, uint8_t key, uint8_t scancode)
{
    /* Handle keystroke */
}

static void my_app_close(window_t *win)
{
    s_my_app_win = NULL;
}

window_t *my_app_launch(void)
{
    if (s_my_app_win) {
        wm_bring_to_front(s_my_app_win);
        wm_set_focus(s_my_app_win);
        return s_my_app_win;
    }

    s_my_app_win = wm_create_window("My Native App", 100, 100, 400, 260, WINDOW_FLAG_RESIZABLE);
    if (s_my_app_win) {
        s_my_app_win->on_paint = my_app_paint;
        s_my_app_win->on_mouse = my_app_mouse;
        s_my_app_win->on_key   = my_app_key;
        s_my_app_win->on_close = my_app_close;
    }
    return s_my_app_win;
}
```

---

## 4. Drawing Primitives Reference

| Function | Description |
| :--- | :--- |
| `draw_pixel(fb, x, y, color)` | Write single pixel respecting clipping boundaries |
| `draw_blend_pixel(fb, x, y, color, alpha)` | Blend pixel with 8-bit alpha (0..255) |
| `draw_line(fb, x0, y0, x1, y1, color)` | Draw Bresenham straight line across all slopes |
| `draw_rect(fb, x, y, w, h, color)` | Draw hollow rectangle outline |
| `draw_fill_rect(fb, x, y, w, h, color)` | Draw solid filled rectangle |
| `draw_rounded_rect(fb, x, y, w, h, r, color)` | Draw rounded rectangle outline |
| `draw_fill_rounded_rect(fb, x, y, w, h, r, color)` | Draw solid filled rounded rectangle |
| `draw_circle(fb, cx, cy, r, color)` | Draw hollow circle outline |
| `draw_fill_circle(fb, cx, cy, r, color)` | Draw solid filled circle |
| `draw_gradient_v(fb, x, y, w, h, top, bottom)` | Draw vertical linear gradient |
| `draw_image(fb, x, y, w, h, pixels, has_alpha)` | Blit bitmap pixel buffer |
| `font_draw_string(fb, x, y, text, scale, color)` | Draw text with scalable 8x8 font |
| `font_draw_char(fb, x, y, ch, scale, color)` | Draw single character or UI icon glyph |
