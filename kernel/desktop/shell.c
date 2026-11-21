/* ZERO KERNEL - Desktop Shell Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/shell.h"
#include "desktop/window.h"
#include "desktop/compositor.h"
#include "desktop/login.h"
#include "desktop/app.h"
#include "desktop/notification.h"
#include "desktop/terminal.h"
#include "desktop/file_manager.h"
#include "desktop/control_center.h"
#include "desktop/sysmon.h"
#include "desktop/about.h"
#include "graphics/cursor.h"
#include "zero/string.h"

static framebuffer_t *s_fb = NULL;
static shell_state_t  s_state = SHELL_STATE_LOGIN;
static uint8_t        s_last_mouse_buttons = 0;

void shell_init(framebuffer_t *fb)
{
    s_fb = fb;
    s_state = SHELL_STATE_LOGIN;
    s_last_mouse_buttons = 0;

    if (fb) {
        cursor_init(fb->width / 2, fb->height / 2);
        wm_init((int32_t)fb->width, (int32_t)fb->height);
        compositor_init(fb);
        login_init(fb);
        notification_init();
        app_init();
    }
}

shell_state_t shell_get_state(void)
{
    return s_state;
}

void shell_set_state(shell_state_t state)
{
    s_state = state;
    if (s_state == SHELL_STATE_DESKTOP) {
        compositor_mark_dirty();
    }
}

void shell_cycle_window(void)
{
    int32_t count = 0;
    window_t **wins = wm_get_windows(&count);
    if (count <= 1) return;

    /* Rotate topmost window to bottom, making previous second window active */
    window_t *top = wins[count - 1];
    for (int32_t i = count - 1; i > 0; i--) {
        wins[i] = wins[i - 1];
        wins[i]->z_order = i;
    }
    wins[0] = top;
    top->z_order = 0;

    wm_set_focus(wins[count - 1]);
    compositor_mark_dirty();
}

void shell_launch_app(app_id_t app_id)
{
    app_launch(app_id);
    compositor_mark_dirty();
}

void shell_dispatch_mouse(int32_t screen_x, int32_t screen_y, uint8_t buttons)
{
    cursor_set_pos(screen_x, screen_y);
    cursor_set_buttons(buttons);

    bool btn_left_down = (buttons & 1u) && !(s_last_mouse_buttons & 1u);
    s_last_mouse_buttons = buttons;

    if (s_state == SHELL_STATE_LOGIN) {
        login_handle_mouse(screen_x, screen_y, buttons);
        if (login_is_authenticated()) {
            s_state = SHELL_STATE_DESKTOP;
            compositor_mark_dirty();
            /* Default welcome environment: launch terminal and control center */
            terminal_launch();
            control_center_launch();
        }
        return;
    }

    if (!s_fb) return;

    if (btn_left_down) {
        int32_t ty = (int32_t)s_fb->height - TASKBAR_HEIGHT;

        /* 1. Check Taskbar Interaction */
        if (screen_y >= ty) {
            /* Start Button: [6 .. 82] */
            if (screen_x >= 6 && screen_x < 82) {
                compositor_toggle_launcher();
                return;
            }

            /* Window Tabs: starting at x = 90 */
            int32_t count = 0;
            window_t **wins = wm_get_windows(&count);
            int32_t tab_x = 90;
            int32_t tab_w = 130;

            for (int32_t i = 0; i < count; i++) {
                if (!wins[i]) continue;
                if (screen_x >= tab_x && screen_x < tab_x + tab_w) {
                    if (wins[i]->flags & WINDOW_FLAG_MINIMIZED) {
                        wm_restore(wins[i]);
                    } else if (wins[i] == wm_get_focused()) {
                        wm_minimize(wins[i]);
                    } else {
                        wm_set_focus(wins[i]);
                    }
                    compositor_mark_dirty();
                    return;
                }
                tab_x += tab_w + 4;
            }

            return;
        }

        /* 2. Check Application Launcher Menu Interaction */
        if (compositor_is_launcher_open()) {
            uint32_t count = 0;
            const app_desc_t *apps = app_get_all(&count);
            int32_t row_h = 28;
            int32_t mw = 220;
            int32_t mh = 36 + (int32_t)count * row_h + 4;
            int32_t mx = 6;
            int32_t my = ty - mh - 4;
            if (my < 10) my = 10;

            if (screen_x >= mx && screen_x < mx + mw && screen_y >= my && screen_y < my + mh) {
                int32_t item_start_y = my + 34;
                if (screen_y >= item_start_y && screen_y < item_start_y + (int32_t)count * row_h) {
                    uint32_t idx = (uint32_t)((screen_y - item_start_y) / row_h);
                    if (idx < count) {
                        shell_launch_app(apps[idx].id);
                    }
                }
                compositor_set_launcher(false);
                return;
            } else {
                /* Click outside closes menu */
                compositor_set_launcher(false);
            }
        }
    }

    /* 3. Forward mouse to Window Manager */
    wm_handle_mouse(screen_x, screen_y, buttons);
}

void shell_dispatch_key(uint8_t key, uint8_t scancode)
{
    if (s_state == SHELL_STATE_LOGIN) {
        login_handle_key(key, scancode);
        if (login_is_authenticated()) {
            s_state = SHELL_STATE_DESKTOP;
            compositor_mark_dirty();
            terminal_launch();
            control_center_launch();
        }
        return;
    }

    /* Global shortcut: Super / Win key or Escape toggles launcher */
    if (scancode == 0x5B || key == 0x1B) {
        compositor_toggle_launcher();
        return;
    }

    /* Global shortcut: Tab with modifier or F6 cycles windows */
    if (scancode == 0x40 /* F6 */) {
        shell_cycle_window();
        return;
    }

    /* Forward key event to focused window */
    wm_handle_key(key, scancode);
}

void shell_tick(void)
{
    if (s_state == SHELL_STATE_LOGIN) {
        login_render(s_fb);
    } else {
        notification_tick(16);
        compositor_render();
    }
}
