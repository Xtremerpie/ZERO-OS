/* ZERO KERNEL - Native Compositor & Scene Renderer Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/compositor.h"
#include "desktop/window.h"
#include "desktop/app.h"
#include "desktop/notification.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "graphics/cursor.h"
#include "zero/string.h"
#include "zero/timer.h"

static framebuffer_t    *s_fb = NULL;
static bool              s_launcher_open = false;
static int32_t           s_launcher_sel = 0;
static bool              s_dirty = true;
static uint64_t          s_last_clock_sec = (uint64_t)-1;
static compositor_stats_t s_stats;

void compositor_init(framebuffer_t *fb)
{
    s_fb = fb;
    s_launcher_open = false;
    s_launcher_sel = 0;
    s_dirty = true;
    s_last_clock_sec = (uint64_t)-1;
    memset(&s_stats, 0, sizeof(compositor_stats_t));

    if (s_fb) {
        fb_reset_clip(s_fb);
        compositor_mark_dirty();
    }
}

void compositor_mark_dirty(void)
{
    s_dirty = true;
    if (s_fb) {
        fb_mark_damage(s_fb, 0, 0, s_fb->width, s_fb->height);
    }
}

void compositor_mark_damage(int32_t x, int32_t y, int32_t w, int32_t h)
{
    s_dirty = true;
    if (s_fb) {
        fb_mark_damage(s_fb, x, y, w, h);
    }
}

void compositor_toggle_launcher(void)
{
    s_launcher_open = !s_launcher_open;
    if (s_launcher_open) s_launcher_sel = 0;
    compositor_mark_dirty();
}

void compositor_set_launcher(bool open)
{
    s_launcher_open = open;
    if (s_launcher_open) s_launcher_sel = 0;
    compositor_mark_dirty();
}

bool compositor_is_launcher_open(void)
{
    return s_launcher_open;
}

void compositor_set_launcher_selection(int32_t index)
{
    s_launcher_sel = index;
    compositor_mark_dirty();
}

int32_t compositor_get_launcher_selection(void)
{
    return s_launcher_sel;
}

static void render_background(void)
{
    if (!s_fb) return;

    /* Fill deep void black */
    draw_fill_rect(s_fb, 0, 0, s_fb->width, s_fb->height - TASKBAR_HEIGHT, COLOR_ZERO_BLACK);

    /* Geometric subtle graphite lattice */
    int32_t step = 48;
    for (int32_t x = 0; x < (int32_t)s_fb->width; x += step) {
        draw_line(s_fb, x, 0, x, s_fb->height - TASKBAR_HEIGHT - 1, 0x0E0E12u);
    }
    for (int32_t y = 0; y < (int32_t)(s_fb->height - TASKBAR_HEIGHT); y += step) {
        draw_line(s_fb, 0, y, s_fb->width - 1, y, 0x0E0E12u);
    }

    /* Subtle geometric gold accent corner brackets */
    draw_line(s_fb, 16, 16, 48, 16, COLOR_ZERO_GOLD_MUTED);
    draw_line(s_fb, 16, 16, 16, 48, COLOR_ZERO_GOLD_MUTED);
    draw_line(s_fb, s_fb->width - 48, 16, s_fb->width - 16, 16, COLOR_ZERO_GOLD_MUTED);
    draw_line(s_fb, s_fb->width - 16, 16, s_fb->width - 16, 48, COLOR_ZERO_GOLD_MUTED);

    /* Central subtle watermark branding */
    int32_t cx = (int32_t)s_fb->width / 2;
    int32_t cy = ((int32_t)s_fb->height - TASKBAR_HEIGHT) / 2;

    const char *brand = "ZERO OS";
    int32_t bw = font_get_string_width(brand, 3);
    int32_t bh = font_get_string_height(3);
    font_draw_string(s_fb, cx - bw / 2, cy - bh / 2 - 12, brand, 3, 0x221E14u);

    const char *tagline = "Beyond Traditional Computing";
    int32_t tw = font_get_string_width(tagline, 1);
    font_draw_string(s_fb, cx - tw / 2, cy + bh / 2 + 4, tagline, 1, 0x24242Eu);
}

static void render_window(window_t *win)
{
    if (!s_fb || !win || !(win->flags & WINDOW_FLAG_VISIBLE) || (win->flags & WINDOW_FLAG_MINIMIZED)) {
        return;
    }

    bool focused = (win->flags & WINDOW_FLAG_FOCUSED) != 0;

    /* Drop shadow */
    draw_fill_rect(s_fb, win->x + 3, win->y + 3, win->width, win->height, 0x040406u);

    /* Window border */
    uint32_t border_col = focused ? COLOR_ZERO_BORDER_FOCUS : COLOR_ZERO_BORDER;
    draw_rect(s_fb, win->x, win->y, win->width, win->height, border_col);

    /* Titlebar */
    uint32_t title_bg = focused ? COLOR_ZERO_TITLEBAR_ACT : COLOR_ZERO_TITLEBAR_INA;
    draw_fill_rect(s_fb, win->x + 1, win->y + 1, win->width - 2, WINDOW_TITLEBAR_HEIGHT - 1, title_bg);

    if (focused) {
        /* 2px gold accent line under titlebar */
        draw_line(s_fb, win->x + 1, win->y + WINDOW_TITLEBAR_HEIGHT - 1,
                  win->x + win->width - 2, win->y + WINDOW_TITLEBAR_HEIGHT - 1, COLOR_ZERO_GOLD);
    }

    /* Window title text */
    uint32_t title_col = focused ? COLOR_ZERO_GOLD : COLOR_ZERO_MUTED;
    rect_t title_clip = { win->x + 8, win->y + 2, win->width - 76, WINDOW_TITLEBAR_HEIGHT - 4 };
    font_draw_string_clipped(s_fb, win->x + 8, win->y + 8, win->title, 1, title_col, &title_clip);

    /* Titlebar control buttons (Minimize, Maximize, Close) */
    int32_t btn_y = win->y + 4;

    /* Minimize button */
    int32_t min_x = win->x + win->width - 66;
    draw_fill_rect(s_fb, min_x, btn_y, WINDOW_BTN_WIDTH, WINDOW_BTN_HEIGHT, 0x1A1A22u);
    draw_rect(s_fb, min_x, btn_y, WINDOW_BTN_WIDTH, WINDOW_BTN_HEIGHT, 0x333340u);
    font_draw_char(s_fb, min_x + 5, btn_y + 5, GLYPH_ICON_MINIMIZE, 1, COLOR_ZERO_MUTED);

    /* Maximize button */
    int32_t max_x = win->x + win->width - 44;
    draw_fill_rect(s_fb, max_x, btn_y, WINDOW_BTN_WIDTH, WINDOW_BTN_HEIGHT, 0x1A1A22u);
    draw_rect(s_fb, max_x, btn_y, WINDOW_BTN_WIDTH, WINDOW_BTN_HEIGHT, 0x333340u);
    font_draw_char(s_fb, max_x + 5, btn_y + 5, GLYPH_ICON_MAXIMIZE, 1, COLOR_ZERO_MUTED);

    /* Close button (Crimson accent) */
    int32_t close_x = win->x + win->width - 22;
    draw_fill_rect(s_fb, close_x, btn_y, WINDOW_BTN_WIDTH, WINDOW_BTN_HEIGHT, 0x4A0F1Au);
    draw_rect(s_fb, close_x, btn_y, WINDOW_BTN_WIDTH, WINDOW_BTN_HEIGHT, COLOR_ZERO_CRIMSON);
    font_draw_char(s_fb, close_x + 5, btn_y + 5, GLYPH_ICON_CLOSE, 1, COLOR_ZERO_WHITE);

    /* Client area */
    int32_t cx = win->x + 1;
    int32_t cy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    if (cw > 0 && ch > 0) {
        fb_set_clip(s_fb, cx, cy, cw, ch);

        if (win->on_paint) {
            win->on_paint(win, s_fb);
        } else if (win->content) {
            draw_image(s_fb, cx, cy, cw, ch, win->content, false);
        } else {
            draw_fill_rect(s_fb, cx, cy, cw, ch, COLOR_ZERO_PANEL);
        }

        fb_reset_clip(s_fb);
    }
}

static void render_taskbar(void)
{
    if (!s_fb) return;

    int32_t ty = s_fb->height - TASKBAR_HEIGHT;

    /* Fill taskbar background */
    draw_fill_rect(s_fb, 0, ty, s_fb->width, TASKBAR_HEIGHT, COLOR_ZERO_TASKBAR);

    /* Top metallic gold border line */
    draw_line(s_fb, 0, ty, s_fb->width - 1, ty, 0x2A2A38u);
    draw_line(s_fb, 0, ty + 1, s_fb->width - 1, ty + 1, COLOR_ZERO_GOLD_MUTED);

    /* "ZERO" Start button */
    uint32_t btn_bg = s_launcher_open ? COLOR_ZERO_PANEL_ALT : 0x16161Du;
    uint32_t btn_border = s_launcher_open ? COLOR_ZERO_GOLD : 0x3A3A4Cu;
    draw_fill_rect(s_fb, 6, ty + 4, 76, 28, btn_bg);
    draw_rect(s_fb, 6, ty + 4, 76, 28, btn_border);
    font_draw_string(s_fb, 18, ty + 14, "ZERO", 1, COLOR_ZERO_GOLD);

    /* Open window tabs on taskbar */
    int32_t count = 0;
    window_t **wins = wm_get_windows(&count);

    int32_t tab_x = 90;
    int32_t tab_w = 130;
    int32_t tab_h = 28;

    for (int32_t i = 0; i < count; i++) {
        window_t *w = wins[i];
        if (!w) continue;

        if (tab_x + tab_w > (int32_t)s_fb->width - 224) {
            break; /* prevent overflow into system tray */
        }

        bool focused = (w->flags & WINDOW_FLAG_FOCUSED) != 0;
        uint32_t tab_bg = focused ? 0x20202Au : 0x14141Cu;
        uint32_t tab_border = focused ? COLOR_ZERO_GOLD : 0x2A2A3Au;

        draw_fill_rect(s_fb, tab_x, ty + 4, tab_w, tab_h, tab_bg);
        draw_rect(s_fb, tab_x, ty + 4, tab_w, tab_h, tab_border);

        if (focused) {
            /* Active gold indicator under tab */
            draw_line(s_fb, tab_x + 2, ty + 4 + tab_h - 2, tab_x + tab_w - 3, ty + 4 + tab_h - 2, COLOR_ZERO_GOLD);
        }

        rect_t tab_clip = { tab_x + 6, ty + 6, tab_w - 12, tab_h - 4 };
        font_draw_string_clipped(s_fb, tab_x + 8, ty + 14, w->title, 1,
                                focused ? COLOR_ZERO_GOLD : COLOR_ZERO_MUTED, &tab_clip);

        tab_x += tab_w + 4;
    }

    /* System tray + Power button at right */
    int32_t tray_x = (int32_t)s_fb->width - 216;

    /* Tray icons: Net, Disk, CPU */
    font_draw_char(s_fb, tray_x, ty + 14, GLYPH_ICON_NET, 1, COLOR_ZERO_SUCCESS);
    font_draw_char(s_fb, tray_x + 18, ty + 14, GLYPH_ICON_DISK, 1, COLOR_ZERO_GOLD);
    font_draw_char(s_fb, tray_x + 36, ty + 14, GLYPH_ICON_CPU, 1, COLOR_ZERO_GOLD);

    /* Live Uptime HH:MM:SS display */
    uint64_t up_sec = timer_get_uptime_ms() / 1000ull;
    uint32_t hh = (uint32_t)((up_sec / 3600ull) % 100ull);
    uint32_t mm = (uint32_t)((up_sec / 60ull) % 60ull);
    uint32_t ss = (uint32_t)(up_sec % 60ull);
    char clk[9];
    clk[0] = (char)('0' + hh / 10); clk[1] = (char)('0' + hh % 10); clk[2] = ':';
    clk[3] = (char)('0' + mm / 10); clk[4] = (char)('0' + mm % 10); clk[5] = ':';
    clk[6] = (char)('0' + ss / 10); clk[7] = (char)('0' + ss % 10); clk[8] = '\0';
    font_draw_string(s_fb, tray_x + 58, ty + 14, clk, 1, COLOR_ZERO_WHITE);

    /* Shutdown [OFF] button at far right [width - 48 .. width - 6] */
    int32_t off_x = (int32_t)s_fb->width - 48;
    draw_fill_rect(s_fb, off_x, ty + 4, 42, 28, 0x4A0F1Au);
    draw_rect(s_fb, off_x, ty + 4, 42, 28, COLOR_ZERO_CRIMSON);
    font_draw_string(s_fb, off_x + 9, ty + 14, "OFF", 1, COLOR_ZERO_WHITE);
}

static void render_launcher_menu(void)
{
    if (!s_fb || !s_launcher_open) return;

    uint32_t count = 0;
    const app_desc_t *apps = app_get_all(&count);
    uint32_t total_items = count + 2u; /* + Lock Screen + Shutdown */

    int32_t row_h = 28;
    int32_t mw = 220;
    int32_t mh = 36 + (int32_t)total_items * row_h + 4;
    int32_t mx = 6;
    int32_t my = (int32_t)s_fb->height - TASKBAR_HEIGHT - mh - 4;
    if (my < 10) my = 10;

    /* Shadow */
    draw_fill_rect(s_fb, mx + 4, my + 4, mw, mh, 0x060608u);

    /* Menu container */
    draw_fill_rect(s_fb, mx, my, mw, mh, COLOR_ZERO_PANEL);
    draw_rect(s_fb, mx, my, mw, mh, COLOR_ZERO_GOLD);

    /* Header */
    draw_fill_rect(s_fb, mx + 1, my + 1, mw - 2, 28, COLOR_ZERO_PANEL_ALT);
    font_draw_string(s_fb, mx + 12, my + 10, "ZERO OS APPS", 1, COLOR_ZERO_GOLD);
    draw_line(s_fb, mx + 1, my + 29, mx + mw - 2, my + 29, COLOR_ZERO_BORDER);

    /* App entries */
    int32_t item_y = my + 34;
    for (uint32_t i = 0; i < count; i++) {
        bool sel = ((int32_t)i == s_launcher_sel);
        draw_fill_rect(s_fb, mx + 4, item_y, mw - 8, 24, sel ? 0x2A2A38u : 0x16161Du);
        draw_rect(s_fb, mx + 4, item_y, mw - 8, 24, sel ? COLOR_ZERO_GOLD : 0x2A2A38u);

        font_draw_char(s_fb, mx + 12, item_y + 8, apps[i].icon_glyph, 1, COLOR_ZERO_GOLD);
        font_draw_string(s_fb, mx + 30, item_y + 8, apps[i].name, 1, sel ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE);

        item_y += row_h;
    }

    /* Lock Screen entry */
    bool lock_sel = ((int32_t)count == s_launcher_sel);
    draw_fill_rect(s_fb, mx + 4, item_y, mw - 8, 24, lock_sel ? 0x2A2A38u : 0x181822u);
    draw_rect(s_fb, mx + 4, item_y, mw - 8, 24, lock_sel ? COLOR_ZERO_GOLD : 0x3A3A4Cu);
    font_draw_char(s_fb, mx + 12, item_y + 8, GLYPH_ICON_GEAR, 1, COLOR_ZERO_GOLD);
    font_draw_string(s_fb, mx + 30, item_y + 8, "Lock Screen", 1, COLOR_ZERO_WHITE);
    item_y += row_h;

    /* Shutdown entry */
    bool shut_sel = ((int32_t)(count + 1u) == s_launcher_sel);
    draw_fill_rect(s_fb, mx + 4, item_y, mw - 8, 24, shut_sel ? 0x5C1422u : 0x3A0D16u);
    draw_rect(s_fb, mx + 4, item_y, mw - 8, 24, shut_sel ? COLOR_ZERO_GOLD : COLOR_ZERO_CRIMSON);
    font_draw_char(s_fb, mx + 12, item_y + 8, GLYPH_ICON_CLOSE, 1, COLOR_ZERO_WHITE);
    font_draw_string(s_fb, mx + 30, item_y + 8, "Shutdown", 1, COLOR_ZERO_WHITE);
}

void compositor_render(void)
{
    if (!s_fb) return;

    uint64_t up_sec = timer_get_uptime_ms() / 1000ull;
    if (up_sec != s_last_clock_sec) {
        s_last_clock_sec = up_sec;
        s_dirty = true;
    }

    if (!s_dirty) return;
    s_dirty = false;

    /* 1. Desktop background */
    render_background();

    /* 2. Window stack in z-order (back to front) */
    int32_t count = 0;
    window_t **wins = wm_get_windows(&count);
    for (int32_t i = 0; i < count; i++) {
        render_window(wins[i]);
    }

    /* 3. System taskbar */
    render_taskbar();

    /* 4. Application launcher menu (if visible) */
    render_launcher_menu();

    /* 5. Desktop toast notifications */
    notification_render(s_fb);

    /* 6. Mouse cursor on top */
    cursor_draw(s_fb);

    /* 7. Flip double buffer to display */
    fb_flip(s_fb);

    s_stats.frames_rendered++;
    s_stats.active_windows = (uint32_t)count;
    s_stats.launcher_open = s_launcher_open;
}

void compositor_get_stats(compositor_stats_t *out_stats)
{
    if (out_stats) {
        *out_stats = s_stats;
    }
}
