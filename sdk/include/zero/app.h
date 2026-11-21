/* ZERO SDK - Application Lifecycle Framework (Milestone 6.0.0 ZERO Ecosystem).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "zero/window.h"
#include "zero/draw.h"
#include "zero/event.h"
#include "zero/syscall.h"

typedef struct zero_app_config {
    const char *app_id;
    const char *name;
    const char *version;
    int32_t     window_width;
    int32_t     window_height;
} zero_app_config_t;

/* Standard application main loop macro */
#define ZERO_APP_MAIN(app_config, on_paint_cb, on_event_cb) \
    int main(int argc, char **argv) { \
        (void)argc; (void)argv; \
        zero_window_t *win = zero_window_create(app_config.name, app_config.window_width, app_config.window_height); \
        if (!win) return 1; \
        win->on_paint = on_paint_cb; \
        win->on_event = on_event_cb; \
        while (win->running) { \
            zero_window_poll_events(win); \
            zero_yield(); \
        } \
        zero_window_destroy(win); \
        return 0; \
    }
