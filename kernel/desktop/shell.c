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
#include "drivers/input.h"
#include "graphics/cursor.h"
#include "storage/bcache.h"
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/selftest.h"
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

static void enter_desktop_session(void)
{
    s_state = SHELL_STATE_DESKTOP;
    compositor_set_launcher(false);
    /* Launch Control Center first and Terminal second so Terminal has focus. */
    control_center_launch();
    terminal_launch();
    compositor_mark_dirty();
}

static void activate_launcher_item(uint32_t idx)
{
    uint32_t count = 0;
    const app_desc_t *apps = app_get_all(&count);
    compositor_set_launcher(false);

    if (idx < count) {
        shell_launch_app(apps[idx].id);
    } else if (idx == count) {
        login_lock();
        s_state = SHELL_STATE_LOGIN;
    } else if (idx == count + 1u) {
        shell_shutdown();
    }
}

__attribute__((noreturn)) void shell_shutdown(void)
{
    KLOG_INFO("power", "ZERO-OS shutting down cleanly...");
    bcache_flush_all();
    /* QEMU Q35 / Bochs / PIIX4 ACPI poweroff ports */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    test_device_exit(true);
    cpu_halt_forever();
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
            enter_desktop_session();
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

            /* Shutdown [OFF] Button: [width - 48 .. width - 6] */
            if (screen_x >= (int32_t)s_fb->width - 48 && screen_x < (int32_t)s_fb->width - 6) {
                shell_shutdown();
            }

            /* Window Tabs: starting at x = 90 */
            int32_t count = 0;
            window_t **wins = wm_get_windows(&count);
            int32_t tab_x = 90;
            int32_t tab_w = 130;

            for (int32_t i = 0; i < count; i++) {
                if (!wins[i]) continue;
                if (tab_x + tab_w > (int32_t)s_fb->width - 224) break;
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

            compositor_mark_dirty();
            return;
        }

        /* 2. Check Application Launcher Menu Interaction */
        if (compositor_is_launcher_open()) {
            uint32_t count = 0;
            (void)app_get_all(&count);
            uint32_t total_items = count + 2u;
            int32_t row_h = 28;
            int32_t mw = 220;
            int32_t mh = 36 + (int32_t)total_items * row_h + 4;
            int32_t mx = 6;
            int32_t my = ty - mh - 4;
            if (my < 10) my = 10;

            if (screen_x >= mx && screen_x < mx + mw && screen_y >= my && screen_y < my + mh) {
                int32_t item_start_y = my + 34;
                if (screen_y >= item_start_y && screen_y < item_start_y + (int32_t)total_items * row_h) {
                    uint32_t idx = (uint32_t)((screen_y - item_start_y) / row_h);
                    if (idx < total_items) {
                        activate_launcher_item(idx);
                        return;
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
    compositor_mark_dirty();
}

void shell_dispatch_key(uint8_t key, uint8_t scancode)
{
    if (s_state == SHELL_STATE_LOGIN) {
        login_handle_key(key, scancode);
        if (login_is_authenticated()) {
            enter_desktop_session();
        }
        return;
    }

    /* Global shortcut: Super / Win key or Escape toggles launcher */
    if (scancode == 0x5B || key == 0x1B) {
        compositor_toggle_launcher();
        return;
    }

    /* Global shortcut: F6 cycles windows */
    if (scancode == 0x40 /* F6 */) {
        shell_cycle_window();
        return;
    }

    /* Forward key event to focused window */
    wm_handle_key(key, scancode);
    compositor_mark_dirty();
}

void shell_dispatch_keycode(uint32_t keycode, uint32_t modifiers)
{
    uint8_t key = (uint8_t)input_keycode_to_ascii(keycode, modifiers);

    if (s_state == SHELL_STATE_LOGIN) {
        login_handle_key(key, (uint8_t)keycode);
        if (login_is_authenticated()) {
            enter_desktop_session();
        }
        return;
    }

    if (keycode == KEY_LWIN || keycode == KEY_RWIN || keycode == KEY_F1 || keycode == KEY_ESCAPE) {
        compositor_toggle_launcher();
        return;
    }

    if (keycode == KEY_F6 || (keycode == KEY_TAB && (modifiers & MODIFIER_ALT))) {
        shell_cycle_window();
        return;
    }

    if (keycode == KEY_F4 && (modifiers & MODIFIER_ALT)) {
        window_t *focused = wm_get_focused();
        if (focused) {
            wm_close(focused);
            compositor_mark_dirty();
        }
        return;
    }

    if (compositor_is_launcher_open()) {
        uint32_t count = 0;
        (void)app_get_all(&count);
        int32_t total = (int32_t)(count + 2u);
        int32_t sel = compositor_get_launcher_selection();

        if (keycode == KEY_UP) {
            sel = (sel - 1 + total) % total;
            compositor_set_launcher_selection(sel);
            return;
        }
        if (keycode == KEY_DOWN || keycode == KEY_TAB) {
            sel = (sel + 1) % total;
            compositor_set_launcher_selection(sel);
            return;
        }
        if (keycode == KEY_ENTER && sel >= 0 && sel < total) {
            activate_launcher_item((uint32_t)sel);
            return;
        }
    }

    wm_handle_key(key, (uint8_t)keycode);
    compositor_mark_dirty();
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
