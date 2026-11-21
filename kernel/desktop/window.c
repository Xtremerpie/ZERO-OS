/* ZERO KERNEL - Window Management Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/window.h"
#include "memory/heap.h"
#include "zero/string.h"
#include "graphics/draw.h"
#include "graphics/font.h"

static window_t *s_windows[MAX_WINDOWS];
static int32_t   s_window_count = 0;
static int32_t   s_next_id = 1;
static int32_t   s_screen_w = 1024;
static int32_t   s_screen_h = 768;
static window_t *s_drag_win = NULL;
static uint8_t   s_last_buttons = 0;

void wm_init(int32_t screen_w, int32_t screen_h)
{
    if (screen_w > 0) s_screen_w = screen_w;
    if (screen_h > 0) s_screen_h = screen_h;
    s_window_count = 0;
    s_next_id = 1;
    s_drag_win = NULL;
    s_last_buttons = 0;

    for (uint32_t i = 0; i < MAX_WINDOWS; i++) {
        s_windows[i] = NULL;
    }
}

window_t *wm_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags)
{
    if (s_window_count >= (int32_t)MAX_WINDOWS) {
        return NULL;
    }

    if (w < WINDOW_MIN_WIDTH)  w = WINDOW_MIN_WIDTH;
    if (h < WINDOW_MIN_HEIGHT) h = WINDOW_MIN_HEIGHT;

    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) {
        return NULL;
    }
    memset(win, 0, sizeof(window_t));

    win->id = s_next_id++;
    if (title) {
        size_t len = strlen(title);
        if (len >= sizeof(win->title)) len = sizeof(win->title) - 1;
        memcpy(win->title, title, len);
        win->title[len] = '\0';
    } else {
        win->title[0] = '\0';
    }

    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;

    win->saved_x = x;
    win->saved_y = y;
    win->saved_width = w;
    win->saved_height = h;

    win->flags = flags | WINDOW_FLAG_VISIBLE;
    win->z_order = s_window_count;

    win->client_width = w - 2;
    win->client_height = h - WINDOW_TITLEBAR_HEIGHT - 1;
    if (win->client_width < 0) win->client_width = 0;
    if (win->client_height < 0) win->client_height = 0;

    size_t buf_bytes = (size_t)win->client_width * (size_t)win->client_height * sizeof(uint32_t);
    if (buf_bytes > 0) {
        win->content = (uint32_t *)kmalloc(buf_bytes);
        if (win->content) {
            memset(win->content, 0, buf_bytes);
        }
    }

    s_windows[s_window_count++] = win;
    wm_set_focus(win);

    return win;
}

void wm_destroy_window(window_t *win)
{
    if (!win) return;

    if (s_drag_win == win) {
        s_drag_win = NULL;
    }

    int32_t idx = -1;
    for (int32_t i = 0; i < s_window_count; i++) {
        if (s_windows[i] == win) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        for (int32_t i = idx; i < s_window_count - 1; i++) {
            s_windows[i] = s_windows[i + 1];
        }
        s_windows[s_window_count - 1] = NULL;
        s_window_count--;
    }

    if (win->content) {
        kfree(win->content);
        win->content = NULL;
    }

    kfree(win);

    /* Focus new top window if available */
    if (s_window_count > 0) {
        wm_set_focus(s_windows[s_window_count - 1]);
    }
}

void wm_bring_to_front(window_t *win)
{
    if (!win || s_window_count <= 1) return;

    int32_t idx = -1;
    for (int32_t i = 0; i < s_window_count; i++) {
        if (s_windows[i] == win) {
            idx = i;
            break;
        }
    }

    if (idx < 0 || idx == s_window_count - 1) {
        return;
    }

    /* Shift windows down and put win at the top */
    for (int32_t i = idx; i < s_window_count - 1; i++) {
        s_windows[i] = s_windows[i + 1];
    }
    s_windows[s_window_count - 1] = win;

    /* Reindex z_order */
    for (int32_t i = 0; i < s_window_count; i++) {
        s_windows[i]->z_order = i;
    }
}

void wm_set_focus(window_t *win)
{
    for (int32_t i = 0; i < s_window_count; i++) {
        if (s_windows[i]) {
            s_windows[i]->flags &= ~WINDOW_FLAG_FOCUSED;
        }
    }

    if (win) {
        win->flags |= WINDOW_FLAG_FOCUSED;
        wm_bring_to_front(win);
    }
}

window_t *wm_get_focused(void)
{
    for (int32_t i = s_window_count - 1; i >= 0; i--) {
        if (s_windows[i] && (s_windows[i]->flags & WINDOW_FLAG_FOCUSED)) {
            return s_windows[i];
        }
    }
    return (s_window_count > 0) ? s_windows[s_window_count - 1] : NULL;
}

void wm_move(window_t *win, int32_t new_x, int32_t new_y)
{
    if (!win || (win->flags & WINDOW_FLAG_MAXIMIZED)) return;

    /* Clamp so at least part of the titlebar stays on screen */
    if (new_y < 0) new_y = 0;
    if (new_y > s_screen_h - WINDOW_TITLEBAR_HEIGHT) new_y = s_screen_h - WINDOW_TITLEBAR_HEIGHT;
    if (new_x < -(win->width - 40)) new_x = -(win->width - 40);
    if (new_x > s_screen_w - 40) new_x = s_screen_w - 40;

    win->x = new_x;
    win->y = new_y;
}

void wm_resize(window_t *win, int32_t new_w, int32_t new_h)
{
    if (!win) return;
    if (new_w < WINDOW_MIN_WIDTH)  new_w = WINDOW_MIN_WIDTH;
    if (new_h < WINDOW_MIN_HEIGHT) new_h = WINDOW_MIN_HEIGHT;

    int32_t new_cw = new_w - 2;
    int32_t new_ch = new_h - WINDOW_TITLEBAR_HEIGHT - 1;
    if (new_cw < 0) new_cw = 0;
    if (new_ch < 0) new_ch = 0;

    size_t new_bytes = (size_t)new_cw * (size_t)new_ch * sizeof(uint32_t);
    uint32_t *new_buf = NULL;
    if (new_bytes > 0) {
        new_buf = (uint32_t *)kmalloc(new_bytes);
        if (new_buf) {
            memset(new_buf, 0, new_bytes);
            /* Copy overlapping region if existing buffer exists */
            if (win->content && win->client_width > 0 && win->client_height > 0) {
                int32_t copy_w = (win->client_width < new_cw) ? win->client_width : new_cw;
                int32_t copy_h = (win->client_height < new_ch) ? win->client_height : new_ch;
                for (int32_t r = 0; r < copy_h; r++) {
                    memcpy(&new_buf[r * new_cw], &win->content[r * win->client_width], (size_t)copy_w * sizeof(uint32_t));
                }
            }
        }
    }

    if (win->content) {
        kfree(win->content);
    }

    win->content = new_buf;
    win->width = new_w;
    win->height = new_h;
    win->client_width = new_cw;
    win->client_height = new_ch;
}

void wm_minimize(window_t *win)
{
    if (!win) return;
    win->flags |= WINDOW_FLAG_MINIMIZED;
    win->flags &= ~WINDOW_FLAG_FOCUSED;

    /* Focus next visible window */
    for (int32_t i = s_window_count - 1; i >= 0; i--) {
        if (s_windows[i] && s_windows[i] != win && !(s_windows[i]->flags & WINDOW_FLAG_MINIMIZED)) {
            wm_set_focus(s_windows[i]);
            break;
        }
    }
}

void wm_maximize(window_t *win)
{
    if (!win) return;
    if (win->flags & WINDOW_FLAG_MAXIMIZED) return;

    win->saved_x = win->x;
    win->saved_y = win->y;
    win->saved_width = win->width;
    win->saved_height = win->height;

    /* Taskbar height is 36px at the bottom */
    win->x = 0;
    win->y = 0;
    win->flags |= WINDOW_FLAG_MAXIMIZED;
    win->flags &= ~WINDOW_FLAG_MINIMIZED;

    wm_resize(win, s_screen_w, s_screen_h - 36);
    wm_set_focus(win);
}

void wm_restore(window_t *win)
{
    if (!win) return;

    if (win->flags & WINDOW_FLAG_MINIMIZED) {
        win->flags &= ~WINDOW_FLAG_MINIMIZED;
        wm_set_focus(win);
        return;
    }

    if (win->flags & WINDOW_FLAG_MAXIMIZED) {
        win->flags &= ~WINDOW_FLAG_MAXIMIZED;
        win->x = win->saved_x;
        win->y = win->saved_y;
        wm_resize(win, win->saved_width, win->saved_height);
        wm_set_focus(win);
    }
}

void wm_close(window_t *win)
{
    if (!win) return;

    if (win->on_close) {
        win->on_close(win);
    }
    wm_destroy_window(win);
}

window_t *wm_get_at(int32_t x, int32_t y)
{
    /* Scan in reverse z-order (front to back) */
    for (int32_t i = s_window_count - 1; i >= 0; i--) {
        window_t *win = s_windows[i];
        if (!win || !(win->flags & WINDOW_FLAG_VISIBLE) || (win->flags & WINDOW_FLAG_MINIMIZED)) {
            continue;
        }

        if (x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height) {
            return win;
        }
    }
    return NULL;
}

wm_hit_region_t wm_hit_test(window_t *win, int32_t x, int32_t y)
{
    if (!win || !(win->flags & WINDOW_FLAG_VISIBLE) || (win->flags & WINDOW_FLAG_MINIMIZED)) {
        return WM_HIT_NONE;
    }

    if (x < win->x || x >= win->x + win->width ||
        y < win->y || y >= win->y + win->height) {
        return WM_HIT_NONE;
    }

    /* Titlebar area */
    if (y < win->y + WINDOW_TITLEBAR_HEIGHT) {
        /* Check buttons at right of titlebar */
        int32_t btn_top = win->y + 4;
        int32_t btn_bottom = win->y + 22;

        if (y >= btn_top && y < btn_bottom) {
            /* Close button: [win->x + win->width - 22 .. win->x + win->width - 4] */
            int32_t close_left = win->x + win->width - 22;
            int32_t close_right = win->x + win->width - 4;
            if (x >= close_left && x < close_right) {
                return WM_HIT_CLOSE;
            }

            /* Maximize button: [win->x + win->width - 44 .. win->x + win->width - 26] */
            int32_t max_left = win->x + win->width - 44;
            int32_t max_right = win->x + win->width - 26;
            if (x >= max_left && x < max_right) {
                return WM_HIT_MAXIMIZE;
            }

            /* Minimize button: [win->x + win->width - 66 .. win->x + win->width - 48] */
            int32_t min_left = win->x + win->width - 66;
            int32_t min_right = win->x + win->width - 48;
            if (x >= min_left && x < min_right) {
                return WM_HIT_MINIMIZE;
            }
        }

        return WM_HIT_TITLEBAR;
    }

    /* Client area */
    if (x >= win->x + 1 && x < win->x + win->width - 1 &&
        y >= win->y + WINDOW_TITLEBAR_HEIGHT && y < win->y + win->height - 1) {
        return WM_HIT_CLIENT;
    }

    return WM_HIT_BORDER;
}

void wm_handle_mouse(int32_t screen_x, int32_t screen_y, uint8_t buttons)
{
    bool btn_left_down = (buttons & 1u) && !(s_last_buttons & 1u);
    bool btn_left_up   = !(buttons & 1u) && (s_last_buttons & 1u);
    s_last_buttons = buttons;

    if (btn_left_down) {
        window_t *hit_win = wm_get_at(screen_x, screen_y);
        if (hit_win) {
            wm_set_focus(hit_win);
            wm_hit_region_t region = wm_hit_test(hit_win, screen_x, screen_y);

            switch (region) {
            case WM_HIT_TITLEBAR:
                if (!(hit_win->flags & WINDOW_FLAG_MAXIMIZED)) {
                    hit_win->flags |= WINDOW_FLAG_DRAGGING;
                    hit_win->drag_offset_x = screen_x - hit_win->x;
                    hit_win->drag_offset_y = screen_y - hit_win->y;
                    s_drag_win = hit_win;
                }
                break;
            case WM_HIT_CLOSE:
                wm_close(hit_win);
                break;
            case WM_HIT_MAXIMIZE:
                if (hit_win->flags & WINDOW_FLAG_MAXIMIZED) {
                    wm_restore(hit_win);
                } else {
                    wm_maximize(hit_win);
                }
                break;
            case WM_HIT_MINIMIZE:
                wm_minimize(hit_win);
                break;
            case WM_HIT_CLIENT:
                if (hit_win->on_mouse) {
                    int32_t lx = screen_x - (hit_win->x + 1);
                    int32_t ly = screen_y - (hit_win->y + WINDOW_TITLEBAR_HEIGHT);
                    hit_win->on_mouse(hit_win, lx, ly, buttons);
                }
                break;
            default:
                break;
            }
        }
    } else if (btn_left_up) {
        if (s_drag_win) {
            s_drag_win->flags &= ~WINDOW_FLAG_DRAGGING;
            s_drag_win = NULL;
        }
    } else {
        /* Mouse movement while dragging */
        if (s_drag_win && (s_drag_win->flags & WINDOW_FLAG_DRAGGING)) {
            wm_move(s_drag_win, screen_x - s_drag_win->drag_offset_x, screen_y - s_drag_win->drag_offset_y);
        } else {
            /* Hover / motion forward to focused window */
            window_t *focused = wm_get_focused();
            if (focused && focused->on_mouse) {
                int32_t lx = screen_x - (focused->x + 1);
                int32_t ly = screen_y - (focused->y + WINDOW_TITLEBAR_HEIGHT);
                if (lx >= 0 && lx < focused->client_width && ly >= 0 && ly < focused->client_height) {
                    focused->on_mouse(focused, lx, ly, buttons);
                }
            }
        }
    }
}

void wm_handle_key(uint8_t key, uint8_t scancode)
{
    window_t *focused = wm_get_focused();
    if (focused && focused->on_key) {
        focused->on_key(focused, key, scancode);
    }
}

window_t **wm_get_windows(int32_t *count)
{
    if (count) {
        *count = s_window_count;
    }
    return s_windows;
}

int32_t wm_get_window_count(void)
{
    return s_window_count;
}

window_t *wm_get_by_id(int32_t id)
{
    for (int32_t i = 0; i < s_window_count; i++) {
        if (s_windows[i] && s_windows[i]->id == id) {
            return s_windows[i];
        }
    }
    return NULL;
}
