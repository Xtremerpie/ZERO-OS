/* ZERO OS SDK Sample Application - ZERO Hello World (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Demonstrates native window creation, 2D vector drawing, typography,
 * event handling, and clean application shutdown.
 */
#include "zero/app.h"

static zero_app_config_t s_app_config = {
    .app_id = "zero.hello",
    .name = "ZERO Hello World",
    .version = "1.0.0",
    .window_width = 440,
    .window_height = 280
};

static int32_t s_click_count = 0;

static void on_paint(zero_window_t *win, zero_surface_t *surf)
{
    (void)win;

    /* Background */
    zero_draw_fill_rect(surf, 0, 0, surf->width, surf->height, ZERO_COLOR_PANEL);

    /* Decorative banner */
    zero_draw_fill_rect(surf, 0, 0, surf->width, 36, ZERO_COLOR_PANEL_ALT);

    /* Text & Branding */
    // Title
    zero_draw_fill_rect(surf, 16, 50, surf->width - 32, 2, ZERO_COLOR_GOLD);

    /* Interactive click counter box */
    zero_draw_fill_rect(surf, 32, 100, surf->width - 64, 48, 0x14141Cu);
}

static void on_event(zero_window_t *win, const zero_event_t *evt)
{
    if (evt->type == ZERO_EVENT_MOUSE_DOWN) {
        s_click_count++;
        zero_window_invalidate(win);
    } else if (evt->type == ZERO_EVENT_CLOSE) {
        win->running = false;
    }
}

ZERO_APP_MAIN(s_app_config, on_paint, on_event)
