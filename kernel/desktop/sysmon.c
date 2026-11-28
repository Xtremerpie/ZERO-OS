/* ZERO KERNEL - ZERO System Monitor Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/sysmon.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "memory/pmm.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "storage/bcache.h"
#include "zero/string.h"
#include "zero/timer.h"
#include <stdio.h>

static window_t *s_sysmon_win = NULL;

static void sysmon_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    struct pmm_stats ps;
    pmm_get_stats(&ps);
    struct scheduler_stats st;
    scheduler_get_stats(&st);
    bcache_stats_t bs;
    bcache_get_stats(&bs);

    char line[96];

    /* Base dark background */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_PANEL);

    /* 1. Header / Resource Overview Section */
    font_draw_string(fb, ox + 14, oy + 12, "RESOURCE UTILIZATION (LIVE)", 1, COLOR_ZERO_GOLD);
    draw_line(fb, ox + 14, oy + 26, ox + cw - 14, oy + 26, COLOR_ZERO_BORDER);

    int32_t meter_x = ox + 14;
    int32_t meter_y = oy + 36;
    int32_t meter_w = cw - 28;

    snprintf(line, sizeof(line), "Timer Ticks: %lu | Context Switches: %lu | Idle: %lu",
             (unsigned long)timer_get_ticks(),
             (unsigned long)st.context_switches,
             (unsigned long)st.idle_ticks);
    font_draw_string(fb, meter_x, meter_y, line, 1, COLOR_ZERO_WHITE);
    draw_fill_rect(fb, meter_x, meter_y + 14, meter_w, 14, 0x14141Cu);
    draw_rect(fb, meter_x, meter_y + 14, meter_w, 14, COLOR_ZERO_BORDER);
    draw_fill_rect(fb, meter_x + 1, meter_y + 15, meter_w / 8, 12, COLOR_ZERO_GOLD);

    /* RAM Meter */
    meter_y += 38;
    uint64_t usable_mib = ps.usable_memory / (1024u * 1024u);
    uint64_t used_mib = ps.allocated_memory / (1024u * 1024u);
    uint64_t pct = usable_mib ? (used_mib * 100ull) / usable_mib : 0;
    snprintf(line, sizeof(line), "RAM Usage: %lu MiB / %lu MiB (%lu%%, 4-Level W^X)",
             (unsigned long)used_mib, (unsigned long)usable_mib, (unsigned long)pct);
    font_draw_string(fb, meter_x, meter_y, line, 1, COLOR_ZERO_WHITE);
    draw_fill_rect(fb, meter_x, meter_y + 14, meter_w, 14, 0x14141Cu);
    draw_rect(fb, meter_x, meter_y + 14, meter_w, 14, COLOR_ZERO_BORDER);
    int32_t ram_fill = (int32_t)((uint64_t)meter_w * pct / 100ull);
    if (ram_fill < 6) ram_fill = 6;
    if (ram_fill > meter_w - 2) ram_fill = meter_w - 2;
    draw_fill_rect(fb, meter_x + 1, meter_y + 15, ram_fill, 12, COLOR_ZERO_SUCCESS);

    /* 2. Process Table */
    int32_t tbl_y = meter_y + 42;
    font_draw_string(fb, meter_x, tbl_y, "ACTIVE KERNEL PROCESSES", 1, COLOR_ZERO_GOLD);
    draw_line(fb, meter_x, tbl_y + 14, meter_x + meter_w, tbl_y + 14, COLOR_ZERO_BORDER);

    int32_t row_y = tbl_y + 20;
    font_draw_string(fb, meter_x + 4, row_y, "PID", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, meter_x + 44, row_y, "NAME", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, meter_x + 210, row_y, "THREADS", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, meter_x + 300, row_y, "STATE", 1, COLOR_ZERO_GOLD);

    row_y += 16;
    int shown = 0;
    for (pid_t pid = 0; pid < (pid_t)MAX_PROCESSES && shown < 5; pid++) {
        process_t *p = process_get(pid);
        if (!p || p->state == PROCESS_STATE_UNUSED) continue;

        draw_fill_rect(fb, meter_x, row_y - 2, meter_w, 18, (shown % 2 == 0) ? 0x16161Eu : 0x1A1A24u);
        char pid_s[12], thr_s[12];
        snprintf(pid_s, sizeof(pid_s), "%d", p->pid);
        snprintf(thr_s, sizeof(thr_s), "%u", p->thread_count);
        font_draw_string(fb, meter_x + 4, row_y + 2, pid_s, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, meter_x + 44, row_y + 2, p->name, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, meter_x + 210, row_y + 2, thr_s, 1, COLOR_ZERO_GOLD);
        font_draw_string(fb, meter_x + 300, row_y + 2, "ACTIVE", 1, COLOR_ZERO_SUCCESS);
        row_y += 20;
        shown++;
    }

    /* 3. Subsystem I/O Counters */
    row_y += 12;
    draw_line(fb, meter_x, row_y, meter_x + meter_w, row_y, COLOR_ZERO_BORDER);
    row_y += 10;
    snprintf(line, sizeof(line), "Block Cache: hits=%lu misses=%lu writes=%lu flushes=%lu",
             (unsigned long)bs.hits, (unsigned long)bs.misses,
             (unsigned long)bs.writes, (unsigned long)bs.flushes);
    font_draw_string(fb, meter_x, row_y, line, 1, COLOR_ZERO_MUTED);
}

static void sysmon_on_close(window_t *win)
{
    (void)win;
    s_sysmon_win = NULL;
}

window_t *sysmon_launch(void)
{
    if (s_sysmon_win) {
        wm_bring_to_front(s_sysmon_win);
        wm_set_focus(s_sysmon_win);
        return s_sysmon_win;
    }

    s_sysmon_win = wm_create_window("ZERO System Monitor", 120, 90, 520, 360, WINDOW_FLAG_RESIZABLE);
    if (s_sysmon_win) {
        s_sysmon_win->on_paint = sysmon_paint;
        s_sysmon_win->on_close = sysmon_on_close;
    }

    return s_sysmon_win;
}

void sysmon_close(void)
{
    if (s_sysmon_win) {
        wm_close(s_sysmon_win);
        s_sysmon_win = NULL;
    }
}

bool sysmon_is_open(void)
{
    return (s_sysmon_win != NULL);
}
