/* ZERO KERNEL - Native Compositor & Scene Renderer (Milestone 5.0.0 ZERO Desktop).
 *
 * Coordinates multi-layer composition:
 * 1. Desktop background & geometric lattice
 * 2. Window stack with shadows, titlebars, and client buffers
 * 3. System taskbar, launcher menu, and tray indicators
 * 4. Hardware mouse cursor overlay
 * 5. Double-buffer page flipping with damage tracking
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "graphics/framebuffer.h"
#include "desktop/window.h"

#define TASKBAR_HEIGHT  36

typedef struct compositor_stats {
    uint64_t frames_rendered;
    uint64_t dirty_flips;
    uint64_t full_flips;
    uint32_t active_windows;
    bool     launcher_open;
} compositor_stats_t;

/* Initialize compositor with active framebuffer */
void compositor_init(framebuffer_t *fb);

/* Invalidate desktop region or request complete frame redraw */
void compositor_mark_dirty(void);
void compositor_mark_damage(int32_t x, int32_t y, int32_t w, int32_t h);

/* Toggle or set application launcher menu visibility */
void compositor_toggle_launcher(void);
void compositor_set_launcher(bool open);
bool compositor_is_launcher_open(void);
void compositor_set_launcher_selection(int32_t index);
int32_t compositor_get_launcher_selection(void);

/* Composite complete scene to backbuffer and flip to display */
void compositor_render(void);

/* Retrieve live compositor diagnostic statistics */
void compositor_get_stats(compositor_stats_t *out_stats);
