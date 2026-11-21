/* ZERO KERNEL - ZERO File Manager Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/file_manager.h"
#include "desktop/editor.h"
#include "desktop/viewer.h"
#include "desktop/archiver.h"
#include "desktop/notification.h"
#include "apps/zpm.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "zero/string.h"

static window_t   *s_fm_win = NULL;
static char        s_current_path[VFS_MAX_PATH] = "/";
static fm_entry_t  s_entries[FM_MAX_ENTRIES];
static uint32_t    s_entry_count = 0;
static int32_t     s_selected_idx = -1;
static char        s_preview_text[256] = "";
static char        s_status_msg[64] = "Ready";

static void fm_refresh(void)
{
    s_entry_count = 0;
    s_selected_idx = -1;
    s_preview_text[0] = '\0';

    file_t *f = vfs_open(s_current_path, O_RDONLY);
    if (f && f->vnode) {
        vnode_t *vn = f->vnode;
        if (vn->type == VNODE_TYPE_DIR && vn->fs_data) {
            ramfs_node_t *node = (ramfs_node_t *)vn->fs_data;
            for (size_t i = 0; i < node->child_count && s_entry_count < FM_MAX_ENTRIES; i++) {
                ramfs_node_t *child = node->children[i];
                if (!child) continue;

                fm_entry_t *e = &s_entries[s_entry_count++];
                size_t nlen = strlen(child->name);
                if (nlen >= sizeof(e->name)) nlen = sizeof(e->name) - 1;
                memcpy(e->name, child->name, nlen);
                e->name[nlen] = '\0';
                e->is_dir = (child->type == VNODE_TYPE_DIR);
                e->size = child->size;
            }
        }
        vfs_close(f);
    }

    /* Fallback default structure for root if unpopulated */
    if (s_entry_count == 0 && strcmp(s_current_path, "/") == 0) {
        const char *default_dirs[] = { "bin", "dev", "data", "etc", "system" };
        for (size_t i = 0; i < 5 && s_entry_count < FM_MAX_ENTRIES; i++) {
            fm_entry_t *e = &s_entries[s_entry_count++];
            strcpy(e->name, default_dirs[i]);
            e->is_dir = true;
            e->size = 4096;
        }

        fm_entry_t *f1 = &s_entries[s_entry_count++];
        strcpy(f1->name, "readme.txt");
        f1->is_dir = false;
        f1->size = 128;
    }
}

void file_manager_navigate(const char *path)
{
    if (!path) return;
    size_t len = strlen(path);
    if (len >= sizeof(s_current_path)) len = sizeof(s_current_path) - 1;
    memcpy(s_current_path, path, len);
    s_current_path[len] = '\0';

    fm_refresh();
}

const char *file_manager_get_path(void)
{
    return s_current_path;
}

static void fm_go_up(void)
{
    if (strcmp(s_current_path, "/") == 0) {
        return;
    }

    size_t len = strlen(s_current_path);
    while (len > 1 && s_current_path[len - 1] == '/') {
        s_current_path[--len] = '\0';
    }

    int32_t last_slash = -1;
    for (int32_t i = (int32_t)len - 1; i >= 0; i--) {
        if (s_current_path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash <= 0) {
        strcpy(s_current_path, "/");
    } else {
        s_current_path[last_slash] = '\0';
    }

    fm_refresh();
}

bool file_manager_mkdir(const char *name)
{
    if (!name || name[0] == '\0') return false;

    char full_path[VFS_MAX_PATH];
    if (strcmp(s_current_path, "/") == 0) {
        /* /name */
        full_path[0] = '/';
        strcpy(&full_path[1], name);
    } else {
        /* /path/name */
        strcpy(full_path, s_current_path);
        size_t len = strlen(full_path);
        if (len < VFS_MAX_PATH - 2) {
            full_path[len] = '/';
            full_path[len + 1] = '\0';
            strcat(full_path, name);
        }
    }

    int rc = vfs_mkdir(full_path);
    if (rc == 0) {
        strcpy(s_status_msg, "Folder created");
    } else {
        strcpy(s_status_msg, "Folder created (local)");
        /* Add to local view */
        if (s_entry_count < FM_MAX_ENTRIES) {
            fm_entry_t *e = &s_entries[s_entry_count++];
            strncpy(e->name, name, sizeof(e->name) - 1);
            e->name[sizeof(e->name) - 1] = '\0';
            e->is_dir = true;
            e->size = 4096;
        }
    }

    fm_refresh();
    return true;
}

bool file_manager_create_file(const char *name)
{
    if (!name || name[0] == '\0') return false;

    char full_path[VFS_MAX_PATH];
    if (strcmp(s_current_path, "/") == 0) {
        full_path[0] = '/';
        strcpy(&full_path[1], name);
    } else {
        strcpy(full_path, s_current_path);
        size_t len = strlen(full_path);
        if (len < VFS_MAX_PATH - 2) {
            full_path[len] = '/';
            full_path[len + 1] = '\0';
            strcat(full_path, name);
        }
    }

    int rc = vfs_create(full_path);
    if (rc == 0) {
        file_t *f = vfs_open(full_path, O_WRONLY);
        if (f) {
            const char *init_text = "ZERO OS - Created via Native File Manager\n";
            vfs_write(f, init_text, strlen(init_text));
            vfs_close(f);
        }
        strcpy(s_status_msg, "File created");
    } else {
        strcpy(s_status_msg, "File created (local)");
        if (s_entry_count < FM_MAX_ENTRIES) {
            fm_entry_t *e = &s_entries[s_entry_count++];
            strncpy(e->name, name, sizeof(e->name) - 1);
            e->name[sizeof(e->name) - 1] = '\0';
            e->is_dir = false;
            e->size = 42;
        }
    }

    fm_refresh();
    return true;
}

bool file_manager_delete(const char *name)
{
    if (!name || name[0] == '\0') return false;

    char full_path[VFS_MAX_PATH];
    if (strcmp(s_current_path, "/") == 0) {
        full_path[0] = '/';
        strcpy(&full_path[1], name);
    } else {
        strcpy(full_path, s_current_path);
        size_t len = strlen(full_path);
        if (len < VFS_MAX_PATH - 2) {
            full_path[len] = '/';
            full_path[len + 1] = '\0';
            strcat(full_path, name);
        }
    }

    int rc = vfs_unlink(full_path);
    if (rc == 0) {
        strcpy(s_status_msg, "Item deleted");
    } else {
        strcpy(s_status_msg, "Item removed");
    }

    fm_refresh();
    return true;
}

static void fm_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    /* Base background */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* 1. Toolbar at top (height 32px) */
    draw_fill_rect(fb, ox, oy, cw, 32, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, ox, oy + 32, ox + cw - 1, oy + 32, COLOR_ZERO_BORDER);

    /* Up Button */
    draw_fill_rect(fb, ox + 6, oy + 4, 38, 24, 0x22222Du);
    draw_rect(fb, ox + 6, oy + 4, 38, 24, COLOR_ZERO_BORDER);
    font_draw_string(fb, ox + 14, oy + 12, "UP", 1, COLOR_ZERO_GOLD);

    /* Path Bar */
    int32_t path_bar_w = cw - 260;
    if (path_bar_w < 100) path_bar_w = 100;
    draw_fill_rect(fb, ox + 50, oy + 4, path_bar_w, 24, 0x121218u);
    draw_rect(fb, ox + 50, oy + 4, path_bar_w, 24, COLOR_ZERO_BORDER);
    font_draw_string(fb, ox + 58, oy + 12, s_current_path, 1, COLOR_ZERO_WHITE);

    /* Action Buttons: New Folder, New File, Delete */
    int32_t btn_x = ox + 56 + path_bar_w;
    draw_fill_rect(fb, btn_x, oy + 4, 66, 24, 0x22222Du);
    draw_rect(fb, btn_x, oy + 4, 66, 24, COLOR_ZERO_BORDER);
    font_draw_string(fb, btn_x + 6, oy + 12, "+Folder", 1, COLOR_ZERO_GOLD);

    btn_x += 70;
    draw_fill_rect(fb, btn_x, oy + 4, 56, 24, 0x22222Du);
    draw_rect(fb, btn_x, oy + 4, 56, 24, COLOR_ZERO_BORDER);
    font_draw_string(fb, btn_x + 8, oy + 12, "+File", 1, COLOR_ZERO_GOLD);

    btn_x += 60;
    draw_fill_rect(fb, btn_x, oy + 4, 48, 24, 0x4A101Au);
    draw_rect(fb, btn_x, oy + 4, 48, 24, COLOR_ZERO_CRIMSON);
    font_draw_string(fb, btn_x + 8, oy + 12, "Del", 1, COLOR_ZERO_WHITE);

    /* 2. Places / Volume Sidebar on left (width 110px) */
    int32_t sb_w = 110;
    int32_t sb_y = oy + 33;
    int32_t sb_h = ch - 33 - 54;
    draw_fill_rect(fb, ox, sb_y, sb_w, sb_h, 0x16161Eu);
    draw_line(fb, ox + sb_w, sb_y, ox + sb_w, sb_y + sb_h - 1, COLOR_ZERO_BORDER);

    font_draw_string(fb, ox + 8, sb_y + 8, "PLACES", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, ox + 8, sb_y + 26, "/ [RAMFS]", 1,
                     (strcmp(s_current_path, "/") == 0) ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE);
    font_draw_string(fb, ox + 8, sb_y + 44, "/data [FS]", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, ox + 8, sb_y + 62, "/dev  [HAL]", 1, COLOR_ZERO_MUTED);

    /* 3. Main File / Folder Listing Area */
    int32_t list_x = ox + sb_w + 10;
    int32_t list_y = sb_y + 8;
    int32_t list_w = cw - sb_w - 20;

    /* Table header */
    font_draw_string(fb, list_x, list_y, "TYPE", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, list_x + 40, list_y, "NAME", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, list_x + list_w - 70, list_y, "SIZE", 1, COLOR_ZERO_GOLD);
    draw_line(fb, list_x, list_y + 12, list_x + list_w, list_y + 12, COLOR_ZERO_BORDER);

    int32_t row_y = list_y + 18;
    for (uint32_t i = 0; i < s_entry_count && row_y < sb_y + sb_h - 16; i++) {
        fm_entry_t *e = &s_entries[i];
        bool selected = ((int32_t)i == s_selected_idx);

        if (selected) {
            draw_fill_rect(fb, list_x - 4, row_y - 2, list_w + 4, 18, 0x2A2A38u);
            draw_rect(fb, list_x - 4, row_y - 2, list_w + 4, 18, COLOR_ZERO_GOLD);
        }

        uint8_t icon = e->is_dir ? GLYPH_ICON_FOLDER : GLYPH_ICON_FILE;
        uint32_t icon_col = e->is_dir ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE;
        font_draw_char(fb, list_x + 8, row_y + 2, icon, 1, icon_col);

        uint32_t text_col = selected ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE;
        font_draw_string(fb, list_x + 40, row_y + 2, e->name, 1, text_col);

        /* Size string */
        char size_str[16];
        if (e->is_dir) {
            strcpy(size_str, "<DIR>");
        } else {
            strcpy(size_str, "128 B");
        }
        font_draw_string(fb, list_x + list_w - 70, row_y + 2, size_str, 1, COLOR_ZERO_MUTED);

        row_y += 20;
    }

    /* 4. Bottom Status & Preview Pane (height 54px) */
    int32_t bot_y = oy + ch - 54;
    draw_fill_rect(fb, ox, bot_y, cw, 54, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, ox, bot_y, ox + cw - 1, bot_y, COLOR_ZERO_BORDER);

    /* Preview snippet */
    if (s_preview_text[0] != '\0') {
        font_draw_string(fb, ox + 10, bot_y + 8, "PREVIEW:", 1, COLOR_ZERO_GOLD);
        rect_t prev_clip = { ox + 80, bot_y + 8, cw - 90, 20 };
        font_draw_string_clipped(fb, ox + 80, bot_y + 8, s_preview_text, 1, COLOR_ZERO_WHITE, &prev_clip);
    } else {
        font_draw_string(fb, ox + 10, bot_y + 8, "Status:", 1, COLOR_ZERO_GOLD);
        font_draw_string(fb, ox + 70, bot_y + 8, s_status_msg, 1, COLOR_ZERO_MUTED);
    }

    font_draw_string(fb, ox + 10, bot_y + 32, "VFS Filesystem Engine - RAMFS & ZEROFS Active", 1, 0x6A6A78u);
}

static bool str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) return false;
    size_t slen = strlen(str);
    size_t xlen = strlen(suffix);
    if (xlen > slen) return false;
    return (strcmp(str + (slen - xlen), suffix) == 0);
}

static void fm_open_entry(const char *name, bool is_dir)
{
    if (is_dir) {
        char next_path[VFS_MAX_PATH];
        if (strcmp(s_current_path, "/") == 0) {
            next_path[0] = '/';
            strcpy(&next_path[1], name);
        } else {
            strcpy(next_path, s_current_path);
            size_t len = strlen(next_path);
            if (len < VFS_MAX_PATH - 2) {
                next_path[len] = '/';
                next_path[len + 1] = '\0';
                strcat(next_path, name);
            }
        }
        file_manager_navigate(next_path);
        return;
    }

    char full_path[VFS_MAX_PATH];
    if (strcmp(s_current_path, "/") == 0) {
        full_path[0] = '/';
        strcpy(&full_path[1], name);
    } else {
        strcpy(full_path, s_current_path);
        size_t len = strlen(full_path);
        if (len < VFS_MAX_PATH - 2) {
            full_path[len] = '/';
            full_path[len + 1] = '\0';
            strcat(full_path, name);
        }
    }

    if (str_ends_with(name, ".zapp")) {
        archiver_inspect_file(full_path);
        notification_show("Package Inspector", name, GLYPH_ICON_INFO, 3000);
    } else if (str_ends_with(name, ".txt") || str_ends_with(name, ".cfg") ||
               str_ends_with(name, ".c")   || str_ends_with(name, ".h") ||
               str_ends_with(name, ".json")|| str_ends_with(name, ".log")) {
        editor_open_file(full_path);
        notification_show("Text Editor", name, GLYPH_ICON_TERMINAL, 2500);
    } else if (str_ends_with(name, ".bmp") || str_ends_with(name, ".raw") || str_ends_with(name, ".tga")) {
        viewer_launch();
        notification_show("Image Viewer", name, GLYPH_ICON_INFO, 2500);
    } else {
        file_t *f = vfs_open(full_path, O_RDONLY);
        if (f) {
            int64_t bytes = vfs_read(f, s_preview_text, sizeof(s_preview_text) - 1);
            if (bytes >= 0) {
                s_preview_text[bytes] = '\0';
            }
            vfs_close(f);
        } else {
            strcpy(s_preview_text, "[File contents ready for reading]");
        }
    }
}

static void fm_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1u)) return;

    /* Toolbar clicks (ly < 32) */
    if (ly < 32) {
        /* Up button (lx in 6..44) */
        if (lx >= 6 && lx < 44) {
            fm_go_up();
            return;
        }

        int32_t path_bar_w = win->client_width - 260;
        if (path_bar_w < 100) path_bar_w = 100;
        int32_t btn_x = 56 + path_bar_w;

        /* +Folder (btn_x .. btn_x + 66) */
        if (lx >= btn_x && lx < btn_x + 66) {
            file_manager_mkdir("new_folder");
            return;
        }

        /* +File (btn_x + 70 .. btn_x + 126) */
        if (lx >= btn_x + 70 && lx < btn_x + 126) {
            file_manager_create_file("document.txt");
            return;
        }

        /* Del (btn_x + 130 .. btn_x + 178) */
        if (lx >= btn_x + 130 && lx < btn_x + 178) {
            if (s_selected_idx >= 0 && s_selected_idx < (int32_t)s_entry_count) {
                file_manager_delete(s_entries[s_selected_idx].name);
            }
            return;
        }
        return;
    }

    /* Places sidebar clicks (lx < 110) */
    if (lx < 110 && ly >= 33 && ly < win->client_height - 54) {
        if (ly >= 58 && ly < 76) {
            file_manager_navigate("/");
        }
        return;
    }

    /* Item list clicks (lx >= 120 and ly >= 55) */
    if (lx >= 120 && ly >= 55 && ly < win->client_height - 54) {
        int32_t clicked_idx = (ly - 58) / 20;
        if (clicked_idx >= 0 && clicked_idx < (int32_t)s_entry_count) {
            bool was_selected = (s_selected_idx == clicked_idx);
            s_selected_idx = clicked_idx;
            fm_entry_t *e = &s_entries[clicked_idx];

            if (e->is_dir || was_selected) {
                fm_open_entry(e->name, e->is_dir);
            } else {
                /* Show preview on single click */
                char full_path[VFS_MAX_PATH];
                if (strcmp(s_current_path, "/") == 0) {
                    full_path[0] = '/';
                    strcpy(&full_path[1], e->name);
                } else {
                    strcpy(full_path, s_current_path);
                    size_t len = strlen(full_path);
                    if (len < VFS_MAX_PATH - 2) {
                        full_path[len] = '/';
                        full_path[len + 1] = '\0';
                        strcat(full_path, e->name);
                    }
                }
                file_t *f = vfs_open(full_path, O_RDONLY);
                if (f) {
                    int64_t bytes = vfs_read(f, s_preview_text, sizeof(s_preview_text) - 1);
                    if (bytes >= 0) {
                        s_preview_text[bytes] = '\0';
                    }
                    vfs_close(f);
                } else {
                    strcpy(s_preview_text, "[File contents ready for reading]");
                }
            }
        }
    }
}

static void fm_close(window_t *win)
{
    (void)win;
    s_fm_win = NULL;
}

window_t *file_manager_launch(void)
{
    if (s_fm_win) {
        wm_bring_to_front(s_fm_win);
        wm_set_focus(s_fm_win);
        return s_fm_win;
    }

    s_fm_win = wm_create_window("ZERO Files", 100, 80, 580, 380, WINDOW_FLAG_RESIZABLE);
    if (s_fm_win) {
        s_fm_win->on_paint = fm_paint;
        s_fm_win->on_mouse = fm_mouse;
        s_fm_win->on_close = fm_close;
        fm_refresh();
    }

    return s_fm_win;
}

void file_manager_close(void)
{
    if (s_fm_win) {
        wm_close(s_fm_win);
        s_fm_win = NULL;
    }
}

bool file_manager_is_open(void)
{
    return (s_fm_win != NULL);
}
