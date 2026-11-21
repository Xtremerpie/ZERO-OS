/* ZERO KERNEL - About ZERO OS Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/about.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "zero/string.h"

static window_t *s_about_win = NULL;

static void about_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    /* Base panel background */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* Geometric corner accents */
    draw_line(fb, ox + 8, oy + 8, ox + 24, oy + 8, COLOR_ZERO_GOLD_MUTED);
    draw_line(fb, ox + 8, oy + 8, ox + 8, oy + 24, COLOR_ZERO_GOLD_MUTED);
    draw_line(fb, ox + cw - 24, oy + 8, ox + cw - 8, oy + 8, COLOR_ZERO_GOLD_MUTED);
    draw_line(fb, ox + cw - 8, oy + 8, ox + cw - 8, oy + 24, COLOR_ZERO_GOLD_MUTED);

    /* Brand Title */
    int32_t cx = ox + cw / 2;
    const char *title = "ZERO OS";
    int32_t tw = font_get_string_width(title, 3);
    font_draw_string(fb, cx - tw / 2, oy + 20, title, 3, COLOR_ZERO_GOLD);

    /* Tagline */
    const char *tagline = "\"Beyond Traditional Computing.\"";
    int32_t tag_w = font_get_string_width(tagline, 1);
    font_draw_string(fb, cx - tag_w / 2, oy + 54, tagline, 1, COLOR_ZERO_WHITE);

    /* Version Badge */
    draw_line(fb, ox + 20, oy + 76, ox + cw - 20, oy + 76, COLOR_ZERO_BORDER);
    const char *ver = "Official Milestone: 6.0.0 ZERO Ecosystem";
    int32_t vw = font_get_string_width(ver, 1);
    font_draw_string(fb, cx - vw / 2, oy + 86, ver, 1, COLOR_ZERO_GOLD);

    /* Specifications */
    int32_t spec_y = oy + 112;
    int32_t spec_x = ox + 32;

    font_draw_string(fb, spec_x, spec_y, "Architecture:  x86_64 Higher-Half Ring 0 Microkernel", 1, COLOR_ZERO_WHITE);
    spec_y += 18;
    font_draw_string(fb, spec_x, spec_y, "Boot Protocol: UEFI Specification via Limine Protocol", 1, COLOR_ZERO_WHITE);
    spec_y += 18;
    font_draw_string(fb, spec_x, spec_y, "Memory Model:  PMM Bitmap + 4-Level Paging (W^X Enforced)", 1, COLOR_ZERO_WHITE);
    spec_y += 18;
    font_draw_string(fb, spec_x, spec_y, "Storage & Net: GPT, ZEROFS v1 Native Filesystem, TCP/IP Stack", 1, COLOR_ZERO_WHITE);
    spec_y += 18;
    font_draw_string(fb, spec_x, spec_y, "Application:   Native .zapp Package Format & ZPM Package Manager", 1, COLOR_ZERO_GOLD);
    spec_y += 18;
    font_draw_string(fb, spec_x, spec_y, "ZERO Store:    Native Graphical App Center & Application Registry", 1, COLOR_ZERO_GOLD);
    spec_y += 18;
    font_draw_string(fb, spec_x, spec_y, "Visual Theme:  Deep Black / Graphite / Gold / Crimson", 1, COLOR_ZERO_WHITE);

    /* Mission statement footer */
    draw_line(fb, ox + 20, oy + ch - 36, ox + cw - 20, oy + ch - 36, COLOR_ZERO_BORDER);
    const char *notice = "An Independent Operating System Built Completely From Scratch.";
    int32_t nw = font_get_string_width(notice, 1);
    font_draw_string(fb, cx - nw / 2, oy + ch - 24, notice, 1, COLOR_ZERO_MUTED);
}

static void about_close(window_t *win)
{
    (void)win;
    s_about_win->on_close = about_on_close;
}

window_t *about_launch(void)
{
    if (s_about_win) {
        wm_bring_to_front(s_about_win);
        wm_set_focus(s_about_win);
        return s_about_win;
    }

    s_about_win = wm_create_window("About ZERO OS", 140, 110, 480, 340, 0);
    if (s_about_win) {
        s_about_win->on_paint = about_paint;
        s_about_win->on_close = about_close;
    }

    return s_about_win;
}

void about_close(void)
{
    if (s_about_win) {
        wm_close(s_about_win);
        s_about_win = NULL;
    }
}

bool about_is_open(void)
{
    return (s_about_win != NULL);
}
