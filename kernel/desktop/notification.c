/* ZERO KERNEL - Native Desktop Notification System Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "desktop/notification.h"
#include "desktop/compositor.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "zero/string.h"

static notification_t s_current;
static bool           s_has_active = false;

static void safe_strcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void notification_init(void)
{
    memset(&s_current, 0, sizeof(s_current));
    s_has_active = false;
}

void notification_show(const char *title, const char *msg, uint8_t icon_glyph, uint32_t duration_ms)
{
    if (!title || !msg) return;

    safe_strcpy(s_current.title, title, sizeof(s_current.title));
    safe_strcpy(s_current.msg, msg, sizeof(s_current.msg));
    s_current.icon_glyph = icon_glyph ? icon_glyph : GLYPH_ICON_INFO;
    s_current.duration_ms = duration_ms ? duration_ms : 3000;
    s_current.elapsed_ms = 0;
    s_current.active = true;
    s_has_active = true;

    compositor_mark_dirty();
}

void notification_dismiss(void)
{
    if (s_has_active) {
        s_has_active = false;
        s_current.active = false;
        compositor_mark_dirty();
    }
}

void notification_tick(uint32_t delta_ms)
{
    if (!s_has_active) return;

    s_current.elapsed_ms += delta_ms;
    if (s_current.elapsed_ms >= s_current.duration_ms) {
        notification_dismiss();
    }
}

bool notification_is_visible(void)
{
    return s_has_active;
}

void notification_render(framebuffer_t *fb)
{
    if (!s_has_active || !fb) return;

    int32_t w = NOTIFICATION_WIDTH;
    int32_t h = NOTIFICATION_HEIGHT;
    int32_t x = (int32_t)fb->width - w - 16;
    int32_t y = (int32_t)fb->height - TASKBAR_HEIGHT - h - 14;

    /* Shadow */
    draw_fill_rect(fb, x + 3, y + 3, w, h, 0x060608u);

    /* Background Panel */
    draw_fill_rect(fb, x, y, w, h, 0x181824u);
    draw_rect(fb, x, y, w, h, COLOR_ZERO_GOLD);

    /* Gold accent bar on left edge */
    draw_fill_rect(fb, x + 1, y + 1, 4, h - 2, COLOR_ZERO_GOLD);

    /* Icon glyph */
    font_draw_char(fb, x + 14, y + 18, s_current.icon_glyph, 1, COLOR_ZERO_GOLD);

    /* Title & Message */
    font_draw_string(fb, x + 34, y + 12, s_current.title, 1, COLOR_ZERO_WHITE);
    font_draw_string(fb, x + 34, y + 26, s_current.msg, 1, COLOR_ZERO_TEXT_DIM);

    /* Remaining duration bar */
    if (s_current.duration_ms > 0 && s_current.elapsed_ms < s_current.duration_ms) {
        int32_t max_bar_w = w - 46;
        int32_t cur_bar_w = (int32_t)((uint64_t)max_bar_w * (s_current.duration_ms - s_current.elapsed_ms) / s_current.duration_ms);
        if (cur_bar_w > 0) {
            draw_line(fb, x + 34, y + 42, x + 34 + cur_bar_w, y + 42, COLOR_ZERO_GOLD);
        }
    }
}
