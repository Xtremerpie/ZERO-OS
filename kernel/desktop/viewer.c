/* ZERO KERNEL - ZERO Image Viewer Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "desktop/viewer.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "zero/string.h"

static window_t *s_view_win = NULL;
static int32_t   s_zoom = 1;
static int32_t   s_pattern_mode = 0; /* 0 = SMPTE color bars, 1 = geometric test, 2 = icon gallery */

static void viewer_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_BLACK);

    /* 1. Toolbar at top (height 30px) */
    draw_fill_rect(fb, ox, oy, cw, 30, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, ox, oy + 30, ox + cw - 1, oy + 30, COLOR_ZERO_BORDER);

    int32_t bx = ox + 8;
    draw_fill_rect(fb, bx, oy + 4, 60, 22, 0x242430u);
    draw_rect(fb, bx, oy + 4, 60, 22, COLOR_ZERO_BORDER);
    font_draw_string(fb, bx + 6, oy + 11, "PATTERN", 1, COLOR_ZERO_GOLD);

    bx += 66;
    draw_fill_rect(fb, bx, oy + 4, 52, 22, 0x242430u);
    draw_rect(fb, bx, oy + 4, 52, 22, COLOR_ZERO_BORDER);
    font_draw_string(fb, bx + 6, oy + 11, "ZOOM+", 1, COLOR_ZERO_WHITE);

    bx += 58;
    draw_fill_rect(fb, bx, oy + 4, 52, 22, 0x242430u);
    draw_rect(fb, bx, oy + 4, 52, 22, COLOR_ZERO_BORDER);
    font_draw_string(fb, bx + 6, oy + 11, "ZOOM-", 1, COLOR_ZERO_WHITE);

    /* Zoom indicator */
    char zstr[16];
    strcpy(zstr, "Zoom: ");
    char zdigit[3] = { (char)('0' + s_zoom), 'x', '\0' };
    strcat(zstr, zdigit);
    font_draw_string(fb, ox + cw - 80, oy + 11, zstr, 1, COLOR_ZERO_GOLD);

    /* 2. Image Canvas Area */
    int32_t cx = ox + 16;
    int32_t cy = oy + 46;
    int32_t iw = cw - 32;
    int32_t ih = ch - 76;

    if (s_pattern_mode == 0) {
        /* SMPTE Color Bars Pattern */
        uint32_t bars[7] = {
            0xFFFFFFu, /* White */
            0xFFFF00u, /* Yellow */
            0x00FFFFu, /* Cyan */
            0x00FF00u, /* Green */
            0xFF00FFu, /* Magenta */
            0xFF0000u, /* Red */
            0x0000FFu  /* Blue */
        };

        int32_t bar_w = iw / 7;
        for (int i = 0; i < 7; i++) {
            draw_fill_rect(fb, cx + i * bar_w, cy, bar_w, ih * 2 / 3, bars[i]);
        }

        /* Bottom grayscale bars */
        for (int i = 0; i < 7; i++) {
            uint8_t g = (uint8_t)(i * 36);
            draw_fill_rect(fb, cx + i * bar_w, cy + ih * 2 / 3, bar_w, ih / 3, color_rgb(g, g, g));
        }
    } else if (s_pattern_mode == 1) {
        /* Geometric Test Pattern */
        draw_rect(fb, cx, cy, iw, ih, COLOR_ZERO_BORDER);
        draw_line(fb, cx, cy, cx + iw - 1, cy + ih - 1, COLOR_ZERO_GOLD);
        draw_line(fb, cx + iw - 1, cy, cx, cy + ih - 1, COLOR_ZERO_GOLD);

        int32_t mid_x = cx + iw / 2;
        int32_t mid_y = cy + ih / 2;
        draw_circle(fb, mid_x, mid_y, 40 * s_zoom, COLOR_ZERO_CRIMSON);
        draw_fill_circle(fb, mid_x, mid_y, 16 * s_zoom, COLOR_ZERO_GOLD);
    } else {
        /* UI Icon Gallery Pattern */
        font_draw_string(fb, cx + 10, cy + 10, "ZERO OS 8x8 NATIVE ICON GLYPHS:", 1, COLOR_ZERO_GOLD);
        uint8_t icon_list[] = {
            GLYPH_ICON_MINIMIZE, GLYPH_ICON_MAXIMIZE, GLYPH_ICON_CLOSE,
            GLYPH_ICON_FOLDER,   GLYPH_ICON_FILE,     GLYPH_ICON_TERMINAL,
            GLYPH_ICON_GEAR,     GLYPH_ICON_CPU,      GLYPH_ICON_DISK,
            GLYPH_ICON_NET,      GLYPH_ICON_CHECK
        };

        int32_t gx = cx + 20;
        int32_t gy = cy + 40;
        for (size_t i = 0; i < sizeof(icon_list); i++) {
            font_draw_char(fb, gx, gy, icon_list[i], (uint32_t)s_zoom, COLOR_ZERO_GOLD);
            gx += 28 * s_zoom;
            if (gx > cx + iw - 30) {
                gx = cx + 20;
                gy += 28 * s_zoom;
            }
        }
    }

    /* 3. Bottom Status Bar */
    int32_t stat_y = oy + ch - 22;
    draw_fill_rect(fb, ox, stat_y, cw, 22, COLOR_ZERO_PANEL);
    draw_line(fb, ox, stat_y, ox + cw - 1, stat_y, COLOR_ZERO_BORDER);

    const char *pname = (s_pattern_mode == 0) ? "Test Pattern: SMPTE Color Bars" :
                        ((s_pattern_mode == 1) ? "Test Pattern: Geometric Crosshair" : "Test Pattern: Icon Matrix");
    font_draw_string(fb, ox + 10, stat_y + 7, pname, 1, COLOR_ZERO_MUTED);
}

static void viewer_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1u)) return;

    if (ly >= 4 && ly < 26) {
        if (lx >= 8 && lx < 68) {
            s_pattern_mode = (s_pattern_mode + 1) % 3;
        } else if (lx >= 74 && lx < 126) {
            if (s_zoom < 4) s_zoom++;
        } else if (lx >= 132 && lx < 184) {
            if (s_zoom > 1) s_zoom--;
        }
    }
}

static void viewer_close_win(window_t *win)
{
    (void)win;
    s_view_win = NULL;
}

void viewer_init(void)
{
    s_view_win = NULL;
    s_zoom = 1;
    s_pattern_mode = 0;
}

window_t *viewer_launch(void)
{
    if (s_view_win) {
        wm_bring_to_front(s_view_win);
        wm_set_focus(s_view_win);
        return s_view_win;
    }

    s_view_win = wm_create_window("ZERO Image Viewer", 130, 85, 520, 360, WINDOW_FLAG_RESIZABLE);
    if (s_view_win) {
        s_view_win->on_paint = viewer_paint;
        s_view_win->on_mouse = viewer_mouse;
        s_view_win->on_close = viewer_close_win;
    }
    return s_view_win;
}

void viewer_close(void)
{
    if (s_view_win) {
        wm_close(s_view_win);
        s_view_win = NULL;
    }
}

bool viewer_is_open(void)
{
    return (s_view_win != NULL);
}
