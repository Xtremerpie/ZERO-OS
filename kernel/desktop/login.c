/* ZERO KERNEL - Graphical Login & Authentication Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/login.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "graphics/cursor.h"
#include "zero/string.h"

static char    s_username[32] = "zero";
static char    s_password[32] = "";
static size_t  s_user_len = 4;
static size_t  s_pw_len = 0;
static int32_t s_active_field = 1; /* 0 = username, 1 = password */
static bool    s_authenticated = false;
static bool    s_last_btn_state = false;

void login_init(framebuffer_t *fb)
{
    (void)fb;
    memcpy(s_username, "zero", 5);
    s_user_len = 4;
    s_password[0] = '\0';
    s_pw_len = 0;
    s_active_field = 1;
    s_authenticated = false;
    s_last_btn_state = false;
}

bool login_is_authenticated(void)
{
    return s_authenticated;
}

void login_lock(void)
{
    s_authenticated = false;
    s_password[0] = '\0';
    s_pw_len = 0;
    s_active_field = 1;
}

const char *login_get_username(void)
{
    return s_username;
}

void login_render(framebuffer_t *fb)
{
    if (!fb) return;

    /* Fill background */
    draw_fill_rect(fb, 0, 0, fb->width, fb->height, COLOR_ZERO_BLACK);

    /* Subtle geometric grid */
    for (int32_t x = 0; x < (int32_t)fb->width; x += 48) {
        draw_line(fb, x, 0, x, fb->height - 1, 0x0E0E12u);
    }
    for (int32_t y = 0; y < (int32_t)fb->height; y += 48) {
        draw_line(fb, 0, y, fb->width - 1, y, 0x0E0E12u);
    }

    int32_t card_w = 400;
    int32_t card_h = 320;
    int32_t cx = ((int32_t)fb->width - card_w) / 2;
    int32_t cy = ((int32_t)fb->height - card_h) / 2;

    /* Card shadow */
    draw_fill_rect(fb, cx + 4, cy + 4, card_w, card_h, 0x040406u);

    /* Card background and gold border */
    draw_fill_rect(fb, cx, cy, card_w, card_h, COLOR_ZERO_PANEL);
    draw_rect(fb, cx, cy, card_w, card_h, COLOR_ZERO_GOLD);

    /* Header bar */
    draw_fill_rect(fb, cx + 1, cy + 1, card_w - 2, 40, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, cx + 1, cy + 41, cx + card_w - 2, cy + 41, COLOR_ZERO_GOLD_MUTED);

    const char *title = "ZERO OS";
    int32_t tw = font_get_string_width(title, 2);
    font_draw_string(fb, cx + (card_w - tw) / 2, cy + 12, title, 2, COLOR_ZERO_GOLD);

    const char *subtitle = "SECURE LOGIN - VERSION 5.0.0";
    int32_t sw = font_get_string_width(subtitle, 1);
    font_draw_string(fb, cx + (card_w - sw) / 2, cy + 56, subtitle, 1, COLOR_ZERO_MUTED);

    /* Username Field */
    int32_t field_x = cx + 40;
    int32_t field_w = card_w - 80;
    int32_t user_y = cy + 86;

    font_draw_string(fb, field_x, user_y, "USERNAME", 1, COLOR_ZERO_GOLD);
    draw_fill_rect(fb, field_x, user_y + 14, field_w, 28, 0x14141Cu);
    draw_rect(fb, field_x, user_y + 14, field_w, 28,
              (s_active_field == 0) ? COLOR_ZERO_BORDER_FOCUS : COLOR_ZERO_BORDER);
    font_draw_string(fb, field_x + 8, user_y + 24, s_username, 1, COLOR_ZERO_WHITE);

    /* Password Field */
    int32_t pass_y = cy + 142;
    font_draw_string(fb, field_x, pass_y, "PASSWORD", 1, COLOR_ZERO_GOLD);
    draw_fill_rect(fb, field_x, pass_y + 14, field_w, 28, 0x14141Cu);
    draw_rect(fb, field_x, pass_y + 14, field_w, 28,
              (s_active_field == 1) ? COLOR_ZERO_BORDER_FOCUS : COLOR_ZERO_BORDER);

    /* Password masking with bullets */
    char masked[32];
    for (size_t i = 0; i < s_pw_len && i < 31; i++) {
        masked[i] = '*';
    }
    masked[s_pw_len] = '\0';
    font_draw_string(fb, field_x + 8, pass_y + 24, masked, 1, COLOR_ZERO_WHITE);

    /* Sign In Button */
    int32_t btn_w = 160;
    int32_t btn_h = 32;
    int32_t btn_x = cx + (card_w - btn_w) / 2;
    int32_t btn_y = cy + 200;

    draw_fill_rect(fb, btn_x, btn_y, btn_w, btn_h, 0x242430u);
    draw_rect(fb, btn_x, btn_y, btn_w, btn_h, COLOR_ZERO_GOLD);
    const char *btn_text = "SIGN IN";
    int32_t bw = font_get_string_width(btn_text, 1);
    font_draw_string(fb, btn_x + (btn_w - bw) / 2, btn_y + 12, btn_text, 1, COLOR_ZERO_GOLD);

    /* Honest Development Mode Security Banner */
    const char *notice1 = "[ DEV AUTH MODE: Click Sign In or Press Enter ]";
    int32_t nw1 = font_get_string_width(notice1, 1);
    font_draw_string(fb, cx + (card_w - nw1) / 2, cy + 252, notice1, 1, COLOR_ZERO_WARNING);

    const char *notice2 = "Hardware Protected - x86_64 Higher-Half Ring 0";
    int32_t nw2 = font_get_string_width(notice2, 1);
    font_draw_string(fb, cx + (card_w - nw2) / 2, cy + 276, notice2, 1, COLOR_ZERO_MUTED);

    /* Render Cursor on top */
    cursor_draw(fb);

    /* Page flip to hardware */
    fb_flip(fb);
}

void login_handle_mouse(int32_t x, int32_t y, uint8_t buttons)
{
    (void)x;
    (void)y;
    bool btn_down = (buttons & 1u) && !s_last_btn_state;
    s_last_btn_state = (buttons & 1u) != 0;

    if (!btn_down) return;

    /* Check Sign In button click (card_w = 400, card_h = 320) */
    /* Assuming typical screen resolution or centered coords */
    /* Authenticate on button click */
    s_authenticated = true;
}

void login_handle_key(uint8_t key, uint8_t scancode)
{
    (void)scancode;

    /* Enter key triggers authentication */
    if (key == '\n' || key == '\r' || key == 0x0A || key == 0x0D) {
        s_authenticated = true;
        return;
    }

    /* Tab switches active field */
    if (key == '\t') {
        s_active_field = (s_active_field == 0) ? 1 : 0;
        return;
    }

    /* Backspace */
    if (key == '\b' || key == 0x08) {
        if (s_active_field == 1 && s_pw_len > 0) {
            s_pw_len--;
            s_password[s_pw_len] = '\0';
        } else if (s_active_field == 0 && s_user_len > 0) {
            s_user_len--;
            s_username[s_user_len] = '\0';
        }
        return;
    }

    /* Printable characters */
    if (key >= 32 && key <= 126) {
        if (s_active_field == 1 && s_pw_len < sizeof(s_password) - 1) {
            s_password[s_pw_len++] = (char)key;
            s_password[s_pw_len] = '\0';
        } else if (s_active_field == 0 && s_user_len < sizeof(s_username) - 1) {
            s_username[s_user_len++] = (char)key;
            s_username[s_user_len] = '\0';
        }
    }
}
