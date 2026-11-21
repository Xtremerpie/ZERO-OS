/* ZERO KERNEL - ZERO Text Editor Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "desktop/editor.h"
#include "fs/vfs.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "zero/string.h"

#define EDITOR_MAX_LINES 20
#define EDITOR_LINE_LEN  64

static window_t *s_edit_win = NULL;
static char      s_filepath[VFS_MAX_PATH] = "/data/notes.txt";
static char      s_lines[EDITOR_MAX_LINES][EDITOR_LINE_LEN];
static int32_t   s_line_count = 1;
static int32_t   s_cur_row = 0;
static int32_t   s_cur_col = 0;
static char      s_status[64] = "Ready";

static void editor_clear(void)
{
    for (int i = 0; i < EDITOR_MAX_LINES; i++) {
        s_lines[i][0] = '\0';
    }
    s_line_count = 1;
    s_cur_row = 0;
    s_cur_col = 0;
    strcpy(s_status, "New file created");
}

static void editor_load_file(const char *path)
{
    if (!path) return;
    strncpy(s_filepath, path, sizeof(s_filepath) - 1);
    s_filepath[sizeof(s_filepath) - 1] = '\0';

    editor_clear();

    file_t *f = vfs_open(path, O_RDONLY);
    if (!f) {
        strcpy(s_status, "File not found (new document)");
        return;
    }

    char buf[1024];
    int64_t bytes = vfs_read(f, buf, sizeof(buf) - 1);
    vfs_close(f);

    if (bytes > 0) {
        buf[bytes] = '\0';
        int row = 0;
        int col = 0;
        for (int64_t i = 0; i < bytes && row < EDITOR_MAX_LINES; i++) {
            if (buf[i] == '\n') {
                s_lines[row][col] = '\0';
                row++;
                col = 0;
            } else if (buf[i] != '\r' && col < EDITOR_LINE_LEN - 1) {
                s_lines[row][col++] = buf[i];
            }
        }
        s_lines[row][col] = '\0';
        s_line_count = (row >= EDITOR_MAX_LINES) ? EDITOR_MAX_LINES : (row + 1);
        strcpy(s_status, "File loaded successfully");
    } else {
        strcpy(s_status, "Empty file loaded");
    }
}

static void editor_save_file(void)
{
    vfs_create(s_filepath);
    file_t *f = vfs_open(s_filepath, O_WRONLY);
    if (!f) {
        strcpy(s_status, "Error: failed to open file for writing");
        return;
    }

    for (int i = 0; i < s_line_count; i++) {
        size_t len = strlen(s_lines[i]);
        if (len > 0) {
            vfs_write(f, s_lines[i], len);
        }
        vfs_write(f, "\n", 1);
    }
    vfs_close(f);

    strcpy(s_status, "File saved successfully");
}

static void editor_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_BLACK);

    /* 1. Toolbar at top (height 30px) */
    draw_fill_rect(fb, ox, oy, cw, 30, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, ox, oy + 30, ox + cw - 1, oy + 30, COLOR_ZERO_BORDER);

    /* Buttons: [New], [Open], [Save] */
    int32_t bx = ox + 8;
    draw_fill_rect(fb, bx, oy + 4, 44, 22, 0x242430u);
    draw_rect(fb, bx, oy + 4, 44, 22, COLOR_ZERO_BORDER);
    font_draw_string(fb, bx + 10, oy + 11, "NEW", 1, COLOR_ZERO_WHITE);

    bx += 50;
    draw_fill_rect(fb, bx, oy + 4, 48, 22, 0x242430u);
    draw_rect(fb, bx, oy + 4, 48, 22, COLOR_ZERO_BORDER);
    font_draw_string(fb, bx + 10, oy + 11, "OPEN", 1, COLOR_ZERO_WHITE);

    bx += 54;
    draw_fill_rect(fb, bx, oy + 4, 48, 22, 0x223824u);
    draw_rect(fb, bx, oy + 4, 48, 22, COLOR_ZERO_SUCCESS);
    font_draw_string(fb, bx + 10, oy + 11, "SAVE", 1, COLOR_ZERO_WHITE);

    /* File path bar */
    bx += 58;
    int32_t pw = cw - (bx - ox) - 10;
    if (pw > 60) {
        draw_fill_rect(fb, bx, oy + 4, pw, 22, 0x14141Cu);
        draw_rect(fb, bx, oy + 4, pw, 22, COLOR_ZERO_BORDER);
        font_draw_string(fb, bx + 8, oy + 11, s_filepath, 1, COLOR_ZERO_GOLD);
    }

    /* 2. Text Area */
    int32_t text_y = oy + 38;
    for (int r = 0; r < s_line_count && text_y < oy + ch - 30; r++) {
        /* Line number */
        char lnum[8];
        lnum[0] = (char)('0' + (r + 1) / 10);
        lnum[1] = (char)('0' + (r + 1) % 10);
        lnum[2] = '\0';
        font_draw_string(fb, ox + 8, text_y, lnum, 1, 0x555566u);

        /* Text line */
        font_draw_string(fb, ox + 36, text_y, s_lines[r], 1, COLOR_ZERO_WHITE);

        /* Caret */
        if (r == s_cur_row) {
            int32_t cw_pos = s_cur_col * 8;
            draw_fill_rect(fb, ox + 36 + cw_pos, text_y + 1, 2, 8, COLOR_ZERO_GOLD);
        }

        text_y += 14;
    }

    /* 3. Bottom Status Bar */
    int32_t stat_y = oy + ch - 22;
    draw_fill_rect(fb, ox, stat_y, cw, 22, COLOR_ZERO_PANEL);
    draw_line(fb, ox, stat_y, ox + cw - 1, stat_y, COLOR_ZERO_BORDER);

    font_draw_string(fb, ox + 10, stat_y + 7, s_status, 1, COLOR_ZERO_MUTED);

    char pos_str[32];
    strcpy(pos_str, "Ln ");
    char rnum[4] = { (char)('0' + (s_cur_row + 1)), '\0' };
    strcat(pos_str, rnum);
    strcat(pos_str, ", Col ");
    char cnum[4] = { (char)('0' + (s_cur_col + 1)), '\0' };
    strcat(pos_str, cnum);
    font_draw_string(fb, ox + cw - 90, stat_y + 7, pos_str, 1, COLOR_ZERO_GOLD);
}

static void editor_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1u)) return;

    /* Toolbar clicks */
    if (ly >= 4 && ly < 26) {
        if (lx >= 8 && lx < 52) {
            editor_clear();
        } else if (lx >= 58 && lx < 106) {
            editor_load_file(s_filepath);
        } else if (lx >= 112 && lx < 160) {
            editor_save_file();
        }
        return;
    }

    /* Text area clicks */
    if (ly >= 38 && ly < win->client_height - 22) {
        int r = (ly - 38) / 14;
        if (r >= 0 && r < s_line_count) {
            s_cur_row = r;
            int c = (lx - 36) / 8;
            size_t len = strlen(s_lines[r]);
            s_cur_col = (c < 0) ? 0 : ((c > (int)len) ? (int)len : c);
        }
    }
}

static void editor_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)win; (void)scancode;

    if (key == '\n' || key == '\r') {
        if (s_line_count < EDITOR_MAX_LINES) {
            for (int i = s_line_count; i > s_cur_row + 1; i--) {
                memcpy(s_lines[i], s_lines[i - 1], EDITOR_LINE_LEN);
            }
            s_cur_row++;
            s_lines[s_cur_row][0] = '\0';
            s_cur_col = 0;
            s_line_count++;
        }
        return;
    }

    if (key == '\b' || key == 0x08) {
        if (s_cur_col > 0) {
            size_t len = strlen(s_lines[s_cur_row]);
            for (size_t i = s_cur_col - 1; i < len; i++) {
                s_lines[s_cur_row][i] = s_lines[s_cur_row][i + 1];
            }
            s_cur_col--;
        } else if (s_cur_row > 0) {
            s_cur_row--;
            s_cur_col = (int32_t)strlen(s_lines[s_cur_row]);
        }
        return;
    }

    if (key >= 32 && key <= 126) {
        size_t len = strlen(s_lines[s_cur_row]);
        if (len < EDITOR_LINE_LEN - 1) {
            for (size_t i = len + 1; i > (size_t)s_cur_col; i--) {
                s_lines[s_cur_row][i] = s_lines[s_cur_row][i - 1];
            }
            s_lines[s_cur_row][s_cur_col] = (char)key;
            s_cur_col++;
        }
    }
}

static void editor_close_win(window_t *win)
{
    (void)win;
    s_edit_win = NULL;
}

void editor_init(void)
{
    s_edit_win = NULL;
    editor_clear();
}

window_t *editor_launch(void)
{
    if (s_edit_win) {
        wm_bring_to_front(s_edit_win);
        wm_set_focus(s_edit_win);
        return s_edit_win;
    }

    s_edit_win = wm_create_window("ZERO Text Editor", 110, 70, 560, 380, WINDOW_FLAG_RESIZABLE);
    if (s_edit_win) {
        s_edit_win->on_paint = editor_paint;
        s_edit_win->on_mouse = editor_mouse;
        s_edit_win->on_key = editor_key;
        s_edit_win->on_close = editor_close_win;

        if (s_lines[0][0] == '\0') {
            strcpy(s_lines[0], "// ZERO OS Native Text Editor");
            strcpy(s_lines[1], "// Milestone 6.0.0 ZERO Ecosystem");
            s_line_count = 2;
            s_cur_row = 1;
            s_cur_col = 34;
        }
    }
    return s_edit_win;
}

window_t *editor_open_file(const char *path)
{
    window_t *w = editor_launch();
    if (w && path) {
        editor_load_file(path);
    }
    return w;
}

void editor_close(void)
{
    if (s_edit_win) {
        wm_close(s_edit_win);
        s_edit_win = NULL;
    }
}

bool editor_is_open(void)
{
    return (s_edit_win != NULL);
}
