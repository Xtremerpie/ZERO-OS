/* ZERO KERNEL - ZERO Software Center (ZERO Store) Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "desktop/store.h"
#include "apps/zpm.h"
#include "desktop/app.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "zero/string.h"

static window_t        *s_store_win = NULL;
static store_category_t s_active_cat = STORE_CAT_ALL;
static char             s_search_query[64] = "";
static char             s_status_text[64] = "Ready | ZPM Package Manager Active";

static const char *s_cat_names[STORE_CAT_COUNT] = {
    "All Apps",
    "Productivity",
    "Development",
    "Utilities",
    "Internet",
    "Media",
    "Installed"
};

void store_init(void)
{
    s_store_win = NULL;
    s_active_cat = STORE_CAT_ALL;
    s_search_query[0] = '\0';
    strcpy(s_status_text, "Ready | ZPM Package Manager Active");
}

void store_set_category(store_category_t cat)
{
    if (cat < STORE_CAT_COUNT) {
        s_active_cat = cat;
    }
}

void store_search(const char *query)
{
    if (query) {
        strncpy(s_search_query, query, sizeof(s_search_query) - 1);
        s_search_query[sizeof(s_search_query) - 1] = '\0';
    } else {
        s_search_query[0] = '\0';
    }
}

static void store_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    /* Base panel background */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* 1. Left Sidebar (Categories) */
    int32_t sb_w = 130;
    draw_fill_rect(fb, ox, oy, sb_w, ch, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, ox + sb_w, oy, ox + sb_w, oy + ch - 1, COLOR_ZERO_BORDER);

    font_draw_string(fb, ox + 12, oy + 12, "CATEGORIES", 1, COLOR_ZERO_GOLD);
    draw_line(fb, ox + 8, oy + 26, ox + sb_w - 8, oy + 26, COLOR_ZERO_BORDER);

    int32_t cat_y = oy + 36;
    for (int32_t i = 0; i < STORE_CAT_COUNT; i++) {
        bool active = (i == (int32_t)s_active_cat);
        if (active) {
            draw_fill_rect(fb, ox + 6, cat_y - 2, sb_w - 12, 24, 0x2A2A38u);
            draw_rect(fb, ox + 6, cat_y - 2, sb_w - 12, 24, COLOR_ZERO_GOLD);
            draw_fill_rect(fb, ox + 6, cat_y + 2, 3, 16, COLOR_ZERO_GOLD);
        }

        font_draw_string(fb, ox + 14, cat_y + 6, s_cat_names[i], 1,
                         active ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE);
        cat_y += 30;
    }

    /* 2. Top Header & Search Bar */
    int32_t mx = ox + sb_w + 14;
    int32_t mw = cw - sb_w - 28;

    font_draw_string(fb, mx, oy + 12, "ZERO STORE - Application Catalog", 1, COLOR_ZERO_GOLD);

    /* Search bar */
    int32_t search_y = oy + 30;
    draw_fill_rect(fb, mx, search_y, mw, 24, 0x14141Cu);
    draw_rect(fb, mx, search_y, mw, 24, COLOR_ZERO_BORDER);
    if (s_search_query[0] != '\0') {
        font_draw_string(fb, mx + 8, search_y + 8, s_search_query, 1, COLOR_ZERO_WHITE);
    } else {
        font_draw_string(fb, mx + 8, search_y + 8, "Search all packages...", 1, COLOR_ZERO_MUTED);
    }

    /* 3. Package Cards List */
    zpm_search_result_t pkgs[8];
    int32_t pkg_count = zpm_search(s_search_query, pkgs, 8);

    int32_t card_y = search_y + 34;
    int32_t card_h = 56;

    for (int32_t i = 0; i < pkg_count && card_y < oy + ch - 40; i++) {
        zpm_search_result_t *p = &pkgs[i];

        /* Filter by category if not ALL and not INSTALLED */
        if (s_active_cat == STORE_CAT_INSTALLED && !p->installed) {
            continue;
        }
        if (s_active_cat != STORE_CAT_ALL && s_active_cat != STORE_CAT_INSTALLED) {
            if (strcmp(p->category, s_cat_names[s_active_cat]) != 0) {
                continue;
            }
        }

        /* Card background */
        draw_fill_rect(fb, mx, card_y, mw, card_h, 0x161620u);
        draw_rect(fb, mx, card_y, mw, card_h, COLOR_ZERO_BORDER);

        /* Icon */
        font_draw_char(fb, mx + 12, card_y + 14, GLYPH_ICON_CHECK, 2, COLOR_ZERO_GOLD);

        /* Title & Version */
        font_draw_string(fb, mx + 40, card_y + 10, p->name, 1, COLOR_ZERO_WHITE);
        char badge[32];
        strcpy(badge, "v");
        strcat(badge, p->version);
        strcat(badge, " [");
        strcat(badge, p->category);
        strcat(badge, "]");
        font_draw_string(fb, mx + 40, card_y + 24, badge, 1, COLOR_ZERO_MUTED);

        /* Description snippet */
        font_draw_string(fb, mx + 40, card_y + 38, p->description, 1, 0x9A9AA6u);

        /* Action Buttons */
        int32_t btn_w = 76;
        int32_t btn_h = 24;
        int32_t btn_rx = mx + mw - btn_w - 10;
        int32_t btn_ry = card_y + 16;

        if (p->installed) {
            /* Launch button */
            draw_fill_rect(fb, btn_rx, btn_ry, btn_w, btn_h, 0x22222Eu);
            draw_rect(fb, btn_rx, btn_ry, btn_w, btn_h, COLOR_ZERO_GOLD);
            font_draw_string(fb, btn_rx + 14, btn_ry + 8, "LAUNCH", 1, COLOR_ZERO_GOLD);

            /* Uninstall button */
            int32_t un_w = 26;
            int32_t un_x = btn_rx - un_w - 6;
            draw_fill_rect(fb, un_x, btn_ry, un_w, btn_h, 0x3A0E16u);
            draw_rect(fb, un_x, btn_ry, un_w, btn_h, COLOR_ZERO_CRIMSON);
            font_draw_char(fb, un_x + 9, btn_ry + 8, GLYPH_ICON_CLOSE, 1, COLOR_ZERO_WHITE);
        } else {
            /* Install button */
            draw_fill_rect(fb, btn_rx, btn_ry, btn_w, btn_h, COLOR_ZERO_GOLD);
            draw_rect(fb, btn_rx, btn_ry, btn_w, btn_h, COLOR_ZERO_GOLD_HOVER);
            font_draw_string(fb, btn_rx + 10, btn_ry + 8, "INSTALL", 1, COLOR_ZERO_BLACK);
        }

        card_y += card_h + 8;
    }

    /* 4. Bottom Status Bar */
    int32_t stat_y = oy + ch - 26;
    draw_fill_rect(fb, ox, stat_y, cw, 26, 0x0E0E14u);
    draw_line(fb, ox, stat_y, ox + cw - 1, stat_y, COLOR_ZERO_BORDER);
    font_draw_string(fb, ox + 14, stat_y + 9, s_status_text, 1, COLOR_ZERO_MUTED);
}

static void store_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1u)) return;

    /* Sidebar category clicks (lx < 130) */
    if (lx < 130 && ly >= 36) {
        int32_t clicked = (ly - 34) / 30;
        if (clicked >= 0 && clicked < (int32_t)STORE_CAT_COUNT) {
            s_active_cat = (store_category_t)clicked;
        }
        return;
    }

    /* Main area action clicks */
    int32_t sb_w = 130;
    int32_t mx = sb_w + 14;
    int32_t mw = win->client_width - sb_w - 28;

    int32_t card_y = 64;
    int32_t card_h = 56;

    zpm_search_result_t pkgs[8];
    int32_t pkg_count = zpm_search(s_search_query, pkgs, 8);

    for (int32_t i = 0; i < pkg_count; i++) {
        zpm_search_result_t *p = &pkgs[i];
        if (s_active_cat == STORE_CAT_INSTALLED && !p->installed) continue;
        if (s_active_cat != STORE_CAT_ALL && s_active_cat != STORE_CAT_INSTALLED) {
            if (strcmp(p->category, s_cat_names[s_active_cat]) != 0) continue;
        }

        if (ly >= card_y && ly < card_y + card_h) {
            int32_t btn_w = 76;
            int32_t btn_rx = mx + mw - btn_w - 10;
            int32_t btn_ry = card_y + 16;

            if (p->installed) {
                /* Launch clicked */
                if (lx >= btn_rx && lx < btn_rx + btn_w && ly >= btn_ry && ly < btn_ry + 24) {
                    strcpy(s_status_text, "Launching ");
                    strcat(s_status_text, p->name);
                    /* Launch app */
                    if (strcmp(p->app_id, "zero.terminal") == 0) app_launch(APP_ID_TERMINAL);
                    else if (strcmp(p->app_id, "zero.files") == 0) app_launch(APP_ID_FILE_MANAGER);
                    else if (strcmp(p->app_id, "zero.control") == 0) app_launch(APP_ID_CONTROL_CENTER);
                    else if (strcmp(p->app_id, "zero.sysmon") == 0) app_launch(APP_ID_SYSTEM_MONITOR);
                    else if (strcmp(p->app_id, "zero.about") == 0) app_launch(APP_ID_ABOUT);
                    return;
                }

                /* Uninstall clicked */
                int32_t un_w = 26;
                int32_t un_x = btn_rx - un_w - 6;
                if (lx >= un_x && lx < un_x + un_w && ly >= btn_ry && ly < btn_ry + 24) {
                    zpm_uninstall(p->app_id);
                    strcpy(s_status_text, "Uninstalled ");
                    strcat(s_status_text, p->name);
                    return;
                }
            } else {
                /* Install clicked */
                if (lx >= btn_rx && lx < btn_rx + btn_w && ly >= btn_ry && ly < btn_ry + 24) {
                    /* Create mock ZAPP buffer to install */
                    uint8_t dummy_pkg[sizeof(zapp_header_t) + sizeof(zapp_manifest_t) + sizeof(zapp_file_entry_t) + 64];
                    memset(dummy_pkg, 0, sizeof(dummy_pkg));

                    zapp_header_t *hdr = (zapp_header_t *)dummy_pkg;
                    hdr->magic = ZAPP_MAGIC;
                    hdr->format_version = ZAPP_FORMAT_VER;
                    strncpy(hdr->app_id, p->app_id, sizeof(hdr->app_id) - 1);
                    strncpy(hdr->app_version, p->version, sizeof(hdr->app_version) - 1);
                    strcpy(hdr->target_arch, "x86_64");
                    hdr->manifest_offset = sizeof(zapp_header_t);
                    hdr->manifest_size = sizeof(zapp_manifest_t);
                    hdr->entries_offset = sizeof(zapp_header_t) + sizeof(zapp_manifest_t);
                    hdr->entry_count = 1;
                    hdr->payload_offset = hdr->entries_offset + sizeof(zapp_file_entry_t);
                    hdr->payload_size = 32;

                    zapp_manifest_t *mf = (zapp_manifest_t *)(dummy_pkg + hdr->manifest_offset);
                    strncpy(mf->name, p->name, sizeof(mf->name) - 1);
                    strncpy(mf->description, p->description, sizeof(mf->description) - 1);
                    strncpy(mf->category, p->category, sizeof(mf->category) - 1);
                    strcpy(mf->exec_path, "bin/app.elf");

                    zapp_file_entry_t *e = (zapp_file_entry_t *)(dummy_pkg + hdr->entries_offset);
                    strcpy(e->path, "bin/app.elf");
                    e->offset = 0;
                    e->size = 32;

                    hdr->checksum = zapp_calc_checksum(dummy_pkg + hdr->payload_offset, hdr->payload_size);

                    zpm_install_buffer(dummy_pkg, sizeof(dummy_pkg));

                    strcpy(s_status_text, "Installed ");
                    strcat(s_status_text, p->name);
                    return;
                }
            }
        }

        card_y += card_h + 8;
    }
}

static void store_close_win(window_t *win)
{
    (void)win;
    s_store_win = NULL;
}

window_t *store_launch(void)
{
    if (s_store_win) {
        wm_bring_to_front(s_store_win);
        wm_set_focus(s_store_win);
        return s_store_win;
    }

    s_store_win = wm_create_window("ZERO Store", 70, 50, 620, 420, WINDOW_FLAG_RESIZABLE);
    if (s_store_win) {
        s_store_win->on_paint = store_paint;
        s_store_win->on_mouse = store_mouse;
        s_store_win->on_close = store_close_win;
    }

    return s_store_win;
}

void store_close(void)
{
    if (s_store_win) {
        wm_close(s_store_win);
        s_store_win = NULL;
    }
}

bool store_is_open(void)
{
    return (s_store_win != NULL);
}
