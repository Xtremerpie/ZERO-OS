/* ZERO KERNEL - ZERO Calculator Application Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "desktop/calc.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "zero/string.h"

static window_t *s_calc_win = NULL;
static char      s_display[32] = "0";
static int64_t   s_acc = 0;
static char      s_op = '\0';
static bool      s_reset_next = true;

static void calc_input_digit(char d)
{
    if (s_reset_next || (s_display[0] == '0' && s_display[1] == '\0')) {
        s_display[0] = d;
        s_display[1] = '\0';
        s_reset_next = false;
    } else {
        size_t len = strlen(s_display);
        if (len < sizeof(s_display) - 1) {
            s_display[len] = d;
            s_display[len + 1] = '\0';
        }
    }
}

static int64_t parse_display(void)
{
    int64_t val = 0;
    int sign = 1;
    const char *p = s_display;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return val * sign;
}

static void format_result(int64_t val)
{
    char tmp[32];
    int idx = 0;
    bool neg = false;
    if (val < 0) {
        neg = true;
        val = -val;
    } else if (val == 0) {
        strcpy(s_display, "0");
        return;
    }

    while (val > 0 && idx < 30) {
        tmp[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    if (neg && idx < 31) {
        tmp[idx++] = '-';
    }

    int pos = 0;
    while (idx > 0) {
        s_display[pos++] = tmp[--idx];
    }
    s_display[pos] = '\0';
}

static void calc_execute_op(void)
{
    int64_t current = parse_display();
    int64_t result = current;

    switch (s_op) {
    case '+': result = s_acc + current; break;
    case '-': result = s_acc - current; break;
    case '*': result = s_acc * current; break;
    case '/': result = (current != 0) ? (s_acc / current) : 0; break;
    default:  break;
    }

    format_result(result);
    s_acc = result;
    s_op = '\0';
    s_reset_next = true;
}

static void calc_button_action(const char *label)
{
    if (label[0] >= '0' && label[0] <= '9') {
        calc_input_digit(label[0]);
    } else if (strcmp(label, "C") == 0) {
        strcpy(s_display, "0");
        s_acc = 0;
        s_op = '\0';
        s_reset_next = true;
    } else if (strcmp(label, "+") == 0 || strcmp(label, "-") == 0 ||
               strcmp(label, "*") == 0 || strcmp(label, "/") == 0) {
        if (s_op != '\0') {
            calc_execute_op();
        } else {
            s_acc = parse_display();
            s_reset_next = true;
        }
        s_op = label[0];
    } else if (strcmp(label, "=") == 0) {
        calc_execute_op();
    } else if (strcmp(label, "+/-") == 0) {
        int64_t v = parse_display();
        format_result(-v);
    }
}

static const char *s_btn_grid[5][4] = {
    { "C", "+/-", "%", "/" },
    { "7", "8",   "9", "*" },
    { "4", "5",   "6", "-" },
    { "1", "2",   "3", "+" },
    { "0", "0",   ".", "=" }
};

static void calc_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* Display Area at top */
    int32_t disp_h = 56;
    draw_fill_rect(fb, ox + 10, oy + 10, cw - 20, disp_h, 0x121218u);
    draw_rect(fb, ox + 10, oy + 10, cw - 20, disp_h, COLOR_ZERO_BORDER);

    /* Operator indicator */
    if (s_op != '\0') {
        char op_str[2] = { s_op, '\0' };
        font_draw_string(fb, ox + 18, oy + 18, op_str, 1, COLOR_ZERO_GOLD);
    }

    /* Result text right-aligned */
    int32_t dw = font_get_string_width(s_display, 2);
    int32_t disp_x = ox + cw - 24 - dw;
    if (disp_x < ox + 36) disp_x = ox + 36;
    font_draw_string(fb, disp_x, oy + 26, s_display, 2, COLOR_ZERO_WHITE);

    /* Button Grid */
    int32_t grid_y = oy + 76;
    int32_t bw = (cw - 20 - 15) / 4;
    int32_t bh = (ch - 86 - 20) / 5;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 0) {
                /* Wide 0 button */
                int32_t bx = ox + 10;
                int32_t by = grid_y + r * (bh + 5);
                int32_t wide_w = bw * 2 + 5;
                draw_fill_rect(fb, bx, by, wide_w, bh, 0x22222Du);
                draw_rect(fb, bx, by, wide_w, bh, COLOR_ZERO_BORDER);
                font_draw_string(fb, bx + wide_w / 2 - 4, by + bh / 2 - 4, "0", 1, COLOR_ZERO_WHITE);
                c++; /* skip next cell */
                continue;
            }

            int32_t bx = ox + 10 + c * (bw + 5);
            int32_t by = grid_y + r * (bh + 5);
            const char *label = s_btn_grid[r][c];

            bool is_op = (c == 3 || strcmp(label, "=") == 0);
            uint32_t bg_col = is_op ? (strcmp(label, "=") == 0 ? COLOR_ZERO_GOLD : 0x2A2A38u) : 0x1A1A22u;
            uint32_t txt_col = (strcmp(label, "=") == 0) ? COLOR_ZERO_BLACK : (is_op ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE);

            draw_fill_rect(fb, bx, by, bw, bh, bg_col);
            draw_rect(fb, bx, by, bw, bh, is_op ? COLOR_ZERO_GOLD : COLOR_ZERO_BORDER);

            int32_t tw = font_get_string_width(label, 1);
            font_draw_string(fb, bx + (bw - tw) / 2, by + bh / 2 - 4, label, 1, txt_col);
        }
    }
}

static void calc_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1u)) return;

    if (ly < 76) return;

    int32_t cw = win->client_width;
    int32_t ch = win->client_height;
    int32_t grid_y = 76;
    int32_t bw = (cw - 20 - 15) / 4;
    int32_t bh = (ch - 86 - 20) / 5;

    int r = (ly - grid_y) / (bh + 5);
    int c = (lx - 10) / (bw + 5);

    if (r >= 0 && r < 5 && c >= 0 && c < 4) {
        if (r == 4 && (c == 0 || c == 1)) {
            calc_button_action("0");
        } else {
            calc_button_action(s_btn_grid[r][c]);
        }
    }
}

static void calc_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)win; (void)scancode;
    if (key >= '0' && key <= '9') {
        char buf[2] = { (char)key, '\0' };
        calc_button_action(buf);
    } else if (key == '+' || key == '-' || key == '*' || key == '/') {
        char buf[2] = { (char)key, '\0' };
        calc_button_action(buf);
    } else if (key == '=' || key == '\r' || key == '\n') {
        calc_button_action("=");
    } else if (key == 'c' || key == 'C' || key == 0x1B) {
        calc_button_action("C");
    }
}

static void calc_close_win(window_t *win)
{
    (void)win;
    s_calc_win = NULL;
}

void calc_init(void)
{
    s_calc_win = NULL;
    calc_reset();
}

window_t *calc_launch(void)
{
    if (s_calc_win) {
        wm_bring_to_front(s_calc_win);
        wm_set_focus(s_calc_win);
        return s_calc_win;
    }

    s_calc_win = wm_create_window("ZERO Calculator", 160, 100, 280, 360, 0);
    if (s_calc_win) {
        s_calc_win->on_paint = calc_paint;
        s_calc_win->on_mouse = calc_mouse;
        s_calc_win->on_key = calc_key;
        s_calc_win->on_close = calc_close_win;
    }
    return s_calc_win;
}

void calc_close(void)
{
    if (s_calc_win) {
        wm_close(s_calc_win);
        s_calc_win = NULL;
    }
}

bool calc_is_open(void)
{
    return (s_calc_win != NULL);
}
