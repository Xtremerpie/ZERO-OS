/* ZERO KERNEL - ZERO Control Center Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/control_center.h"
#include "desktop/compositor.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "hardware/acpi.h"
#include "memory/pmm.h"
#include "zero/string.h"
#include "zero/version.h"
#include <stdio.h>

static window_t *s_cc_win = NULL;
static cc_tab_t  s_active_tab = CC_TAB_DISPLAY;

static const char *s_tab_names[CC_TAB_COUNT] = {
    "Display",
    "Input",
    "System",
    "Network",
    "Storage",
    "Themes",
    "Permissions"
};

static const uint8_t s_tab_icons[CC_TAB_COUNT] = {
    GLYPH_ICON_MAXIMIZE,
    GLYPH_ICON_CLOSE,
    GLYPH_ICON_CPU,
    GLYPH_ICON_NET,
    GLYPH_ICON_DISK,
    GLYPH_ICON_GEAR,
    GLYPH_ICON_CHECK
};

static void cc_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    /* Base panel background */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* Left Sidebar */
    int32_t sidebar_w = 140;
    draw_fill_rect(fb, ox, oy, sidebar_w, ch, COLOR_ZERO_PANEL_ALT);
    draw_line(fb, ox + sidebar_w, oy, ox + sidebar_w, oy + ch - 1, COLOR_ZERO_BORDER);

    /* Draw Sidebar Tab Buttons */
    int32_t tab_y = oy + 12;
    for (int32_t i = 0; i < CC_TAB_COUNT; i++) {
        bool active = (i == (int32_t)s_active_tab);
        uint32_t tab_bg = active ? 0x2A2A38u : 0x1A1A24u;
        uint32_t tab_border = active ? COLOR_ZERO_GOLD : 0x242430u;

        draw_fill_rect(fb, ox + 8, tab_y, sidebar_w - 16, 32, tab_bg);
        draw_rect(fb, ox + 8, tab_y, sidebar_w - 16, 32, tab_border);

        if (active) {
            /* Gold left accent marker */
            draw_fill_rect(fb, ox + 8, tab_y + 4, 3, 24, COLOR_ZERO_GOLD);
        }

        /* Icon and Tab Name */
        font_draw_char(fb, ox + 18, tab_y + 12, s_tab_icons[i], 1,
                       active ? COLOR_ZERO_GOLD : COLOR_ZERO_MUTED);
        font_draw_string(fb, ox + 36, tab_y + 12, s_tab_names[i], 1,
                         active ? COLOR_ZERO_WHITE : COLOR_ZERO_MUTED);

        tab_y += 38;
    }

    /* Main Content Area */
    int32_t mx = ox + sidebar_w + 20;
    int32_t my = oy + 16;
    int32_t mw = cw - sidebar_w - 30;

    /* Tab Title Header */
    font_draw_string(fb, mx, my, s_tab_names[s_active_tab], 2, COLOR_ZERO_GOLD);
    draw_line(fb, mx, my + 24, mx + mw, my + 24, COLOR_ZERO_BORDER);

    int32_t row_y = my + 38;
    int32_t row_h = 24;

    switch (s_active_tab) {
    case CC_TAB_DISPLAY: {
        char disp_line[64];
        snprintf(disp_line, sizeof(disp_line), "Resolution:      %u x %u @ %u bpp (RGB)",
                 fb->width, fb->height, fb->bpp);
        font_draw_string(fb, mx, row_y, disp_line, 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y,
                         (fb->back_buffer != fb->front_buffer)
                             ? "Frame Engine:    Double-Buffered RAM Surface"
                             : "Frame Engine:    Direct Hardware VRAM",
                         1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Damage Tracking: Active (Bounding Box Dirty Blits)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Hardware VRAM:   Linear Framebuffer (Limine GOP)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "GPU Accel:       Software Blitter (No HW 2D/3D)", 1, COLOR_ZERO_MUTED);
        break;
    }

    case CC_TAB_INPUT:
        font_draw_string(fb, mx, row_y, "Mouse Driver:    PS/2 IRQ 12 (3-Byte Packet Stream)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Delta Precision: 9-bit Signed Two's Complement X/Y", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Keyboard Driver: PS/2 IRQ 1 (Scancode Set 1)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Pointer Overlay: 16x16 Vector Gold Arrow", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Global Hotkeys:  F1/Esc/Win (Menu), Alt+Tab/F6, Alt+F4", 1, COLOR_ZERO_WHITE);
        break;

    case CC_TAB_SYSTEM: {
        struct pmm_stats ps;
        pmm_get_stats(&ps);
        const struct acpi_madt_info *madt = acpi_get_madt_info();
        uint32_t cpus = (madt && madt->cpu_count > 0) ? madt->cpu_count : 1;
        char sys_line[64];
        font_draw_string(fb, mx, row_y, "Architecture:    x86_64 Higher-Half Ring 0 Kernel", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        snprintf(sys_line, sizeof(sys_line), "MADT CPUs:       %u Logical Processor(s)", cpus);
        font_draw_string(fb, mx, row_y, sys_line, 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Memory Model:    PMM + 4-Level VMM with W^X Protection", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        snprintf(sys_line, sizeof(sys_line), "Usable RAM:      %lu MiB (%lu MiB Free)",
                 (unsigned long)(ps.usable_memory / (1024u * 1024u)),
                 (unsigned long)(ps.free_memory / (1024u * 1024u)));
        font_draw_string(fb, mx, row_y, sys_line, 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Operating OS:    ZERO OS " ZERO_VERSION_STRING " " ZERO_CODENAME, 1, COLOR_ZERO_GOLD);
        break;
    }

    case CC_TAB_NETWORK:
        font_draw_string(fb, mx, row_y, "Controller:      Intel 82540EM (e1000) Local Driver", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "MAC Address:     52:54:00:12:34:56", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "IPv4 Config:     10.0.2.15 (Subnet: 255.255.255.0)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Gateway / DNS:   10.0.2.2 / 10.0.2.3", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Status:          Stack Ready (External Internet Offline)", 1, COLOR_ZERO_WARNING);
        break;

    case CC_TAB_STORAGE:
        font_draw_string(fb, mx, row_y, "Block Controller: In-Memory RAM Disk (ramdisk0)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Partition Table: GPT / MBR Parsers Active", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Native FS:       ZEROFS v1 (4096B block cache, 128B inodes)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Root Mount [/]:  RAMFS In-Memory Virtual Filesystem", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Data Mount [/data]: ZEROFS on ramdisk0 (Volatile)", 1, COLOR_ZERO_GOLD);
        break;

    case CC_TAB_THEMES:
        font_draw_string(fb, mx, row_y, "Active Theme:    Dark Abyss (Official ZERO Palette)", 1, COLOR_ZERO_GOLD);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Primary Colors:  Deep Void Black (#08080A), Graphite (#121216)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Accent Colors:   Metallic Gold (#D4AF37), Crimson Red (#C41230)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Typography:      Scalable 8x8 Matrix Font + Custom Icon Glyphs", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Window Styling:  Geometric 1px Borders, Active Accent Titlebars", 1, COLOR_ZERO_WHITE);
        break;

    case CC_TAB_PERMISSIONS:
        font_draw_string(fb, mx, row_y, "Sandbox Model:   Per-App Capability Grants (Ring 3 Ring Boundary)", 1, COLOR_ZERO_GOLD);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Storage Bounds:  Sandboxed to /data/apps/<id> and /data/config", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Network Access:  BSD Sockets Syscalls (CAP_NET Policy Regulated)", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Display Buffer:  Window Surface Damage Clipping & Composite", 1, COLOR_ZERO_WHITE);
        row_y += row_h;
        font_draw_string(fb, mx, row_y, "Package Security: SHA-256 Digest & CRC32 Header Verification", 1, COLOR_ZERO_WHITE);
        break;

    default:
        break;
    }
}

static void cc_mouse(window_t *win, int32_t lx, int32_t ly, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1u)) return;

    /* Check sidebar clicks (lx < 140) */
    if (lx >= 8 && lx < 132) {
        int32_t clicked = (ly - 12) / 38;
        if (clicked >= 0 && clicked < (int32_t)CC_TAB_COUNT) {
            s_active_tab = (cc_tab_t)clicked;
            compositor_mark_dirty();
        }
    }
}

static void cc_close(window_t *win)
{
    (void)win;
    s_cc_win = NULL;
}

window_t *control_center_launch(void)
{
    if (s_cc_win) {
        wm_bring_to_front(s_cc_win);
        wm_set_focus(s_cc_win);
        return s_cc_win;
    }

    s_cc_win = wm_create_window("ZERO Control Center", 640, 60, 580, 360, WINDOW_FLAG_RESIZABLE);
    if (s_cc_win) {
        s_cc_win->on_paint = cc_paint;
        s_cc_win->on_mouse = cc_mouse;
        s_cc_win->on_close = cc_close;
    }

    return s_cc_win;
}

void control_center_close(void)
{
    if (s_cc_win) {
        wm_close(s_cc_win);
        s_cc_win = NULL;
    }
}

bool control_center_is_open(void)
{
    return (s_cc_win != NULL);
}

cc_tab_t control_center_get_active_tab(void)
{
    return s_active_tab;
}

void control_center_set_active_tab(cc_tab_t tab)
{
    if (tab < CC_TAB_COUNT) {
        s_active_tab = tab;
    }
}
