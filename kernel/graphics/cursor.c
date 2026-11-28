/* ZERO KERNEL - Mouse Cursor Implementation (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements 16x16 pointer arrow rendering, background restoration, position clamping,
 * and button state tracking.
 */
#include "graphics/cursor.h"
#include "graphics/draw.h"
#include "graphics/color.h"
#include "zero/spinlock.h"
#include "lib/string.h"

/* 16x16 Futuristic Pointer Arrow Bitmap
 * '.' = transparent, '#' = black border, 'G' = metallic gold, 'W' = white accent */
static const char *cursor_bitmap[CURSOR_HEIGHT] = {
    "#...............",
    "##..............",
    "#W#.............",
    "#WW#............",
    "#WGG#...........",
    "#WGGG#..........",
    "#WGGGG#.........",
    "#WGGGGG#........",
    "#WGGGGGG#.......",
    "#WGGGG###.......",
    "#WGG#G#.........",
    "#WG#.#G#........",
    "##...#G#........",
    "#.....#G#.......",
    ".......#G#......",
    "........##......"
};

static mouse_cursor_t g_cursor;
static spinlock_t     g_cursor_lock;

void cursor_init(uint32_t start_x, uint32_t start_y)
{
    spinlock_init(&g_cursor_lock);
    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    memset(&g_cursor, 0, sizeof(g_cursor));
    g_cursor.x = (int32_t)start_x;
    g_cursor.y = (int32_t)start_y;
    g_cursor.prev_x = g_cursor.x;
    g_cursor.prev_y = g_cursor.y;
    g_cursor.visible = true;
    g_cursor.has_saved_bg = false;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_set_pos(int32_t x, int32_t y)
{
    framebuffer_t *fb = fb_get_primary();
    int32_t max_x = fb ? (int32_t)fb->width - 1 : 1920;
    int32_t max_y = fb ? (int32_t)fb->height - 1 : 1080;

    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    g_cursor.prev_x = g_cursor.x;
    g_cursor.prev_y = g_cursor.y;

    if (x < 0) x = 0;
    if (x > max_x) x = max_x;
    if (y < 0) y = 0;
    if (y > max_y) y = max_y;

    g_cursor.x = x;
    g_cursor.y = y;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_move_delta(int32_t dx, int32_t dy)
{
    framebuffer_t *fb = fb_get_primary();
    int32_t max_x = fb ? (int32_t)fb->width - 1 : 1920;
    int32_t max_y = fb ? (int32_t)fb->height - 1 : 1080;

    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    g_cursor.prev_x = g_cursor.x;
    g_cursor.prev_y = g_cursor.y;

    int32_t nx = g_cursor.x + dx;
    int32_t ny = g_cursor.y + dy;

    if (nx < 0) nx = 0;
    if (nx > max_x) nx = max_x;
    if (ny < 0) ny = 0;
    if (ny > max_y) ny = max_y;

    g_cursor.x = nx;
    g_cursor.y = ny;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_set_buttons(uint8_t buttons)
{
    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    g_cursor.prev_buttons = g_cursor.buttons;
    g_cursor.buttons = buttons;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_set_visible(bool visible)
{
    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    g_cursor.visible = visible;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_get_state(int32_t *out_x, int32_t *out_y, uint8_t *out_buttons)
{
    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    if (out_x) *out_x = g_cursor.x;
    if (out_y) *out_y = g_cursor.y;
    if (out_buttons) *out_buttons = g_cursor.buttons;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_restore_bg(framebuffer_t *fb)
{
    if (!fb || !fb->initialized) return;

    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    if (!g_cursor.has_saved_bg) {
        spin_unlock_irqrestore(&g_cursor_lock, flags);
        return;
    }

    int32_t sx = g_cursor.saved_x;
    int32_t sy = g_cursor.saved_y;

    for (uint32_t row = 0; row < CURSOR_HEIGHT; row++) {
        int32_t py = sy + (int32_t)row;
        if (py < 0 || (uint32_t)py >= fb->height) continue;

        for (uint32_t col = 0; col < CURSOR_WIDTH; col++) {
            int32_t px = sx + (int32_t)col;
            if (px < 0 || (uint32_t)px >= fb->width) continue;

            uint32_t bg_val = g_cursor.saved_bg[row * CURSOR_WIDTH + col];
            fb->back_buffer[(uint32_t)py * fb->pixels_per_pitch + (uint32_t)px] = bg_val;
        }
    }

    fb_mark_damage(fb, sx, sy, CURSOR_WIDTH, CURSOR_HEIGHT);
    g_cursor.has_saved_bg = false;
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}

void cursor_draw(framebuffer_t *fb)
{
    if (!fb || !fb->initialized) return;

    uint64_t flags;
    spin_lock_irqsave(&g_cursor_lock, &flags);
    if (!g_cursor.visible) {
        spin_unlock_irqrestore(&g_cursor_lock, flags);
        return;
    }

    int32_t cx = g_cursor.x;
    int32_t cy = g_cursor.y;

    g_cursor.saved_x = cx;
    g_cursor.saved_y = cy;
    g_cursor.has_saved_bg = true;

    /* 1. Save background pixels underneath cursor */
    for (uint32_t row = 0; row < CURSOR_HEIGHT; row++) {
        int32_t py = cy + (int32_t)row;
        for (uint32_t col = 0; col < CURSOR_WIDTH; col++) {
            int32_t px = cx + (int32_t)col;
            if (px >= 0 && (uint32_t)px < fb->width && py >= 0 && (uint32_t)py < fb->height) {
                g_cursor.saved_bg[row * CURSOR_WIDTH + col] = fb->back_buffer[(uint32_t)py * fb->pixels_per_pitch + (uint32_t)px];
            } else {
                g_cursor.saved_bg[row * CURSOR_WIDTH + col] = 0;
            }
        }
    }

    /* 2. Render cursor arrow */
    for (uint32_t row = 0; row < CURSOR_HEIGHT; row++) {
        int32_t py = cy + (int32_t)row;
        if (py < 0 || (uint32_t)py >= fb->height) continue;

        const char *line = cursor_bitmap[row];
        for (uint32_t col = 0; col < CURSOR_WIDTH; col++) {
            int32_t px = cx + (int32_t)col;
            if (px < 0 || (uint32_t)px >= fb->width) continue;

            char ch = line[col];
            uint32_t color = 0;
            if (ch == '#') {
                color = 0x050505u; /* Deep black outline */
            } else if (ch == 'G') {
                color = COLOR_ZERO_GOLD; /* Metallic gold core */
            } else if (ch == 'W') {
                color = 0xFFFFFFu; /* White highlight edge */
            } else {
                continue; /* Transparent */
            }

            draw_pixel(fb, px, py, color);
        }
    }

    fb_mark_damage(fb, cx, cy, CURSOR_WIDTH, CURSOR_HEIGHT);
    spin_unlock_irqrestore(&g_cursor_lock, flags);
}
