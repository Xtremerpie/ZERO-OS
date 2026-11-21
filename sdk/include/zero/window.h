/* ZERO SDK - Window Management Interface (Milestone 6.0.0 ZERO Ecosystem).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "zero/draw.h"
#include "zero/event.h"

typedef struct zero_window zero_window_t;

typedef void (*zero_paint_fn)(zero_window_t *win, zero_surface_t *surf);
typedef void (*zero_event_fn)(zero_window_t *win, const zero_event_t *evt);

struct zero_window {
    int32_t        id;
    char           title[64];
    int32_t        width;
    int32_t        height;
    zero_surface_t surface;
    zero_paint_fn  on_paint;
    zero_event_fn  on_event;
    void          *user_data;
    bool           running;
};

/* Application Window Lifecycle */
zero_window_t *zero_window_create(const char *title, int32_t w, int32_t h);
void zero_window_destroy(zero_window_t *win);
void zero_window_poll_events(zero_window_t *win);
void zero_window_invalidate(zero_window_t *win);
