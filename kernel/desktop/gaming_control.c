/* ZERO KERNEL - ZERO Gaming Control Center Implementation (Milestone 9.0.0 ZERO Ultimate). */

#include "desktop/gaming_control.h"
#include "desktop/window.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "gaming/gaming.h"
#include "process/scheduler.h"
#include "graphics/gpu.h"
#include "drivers/audio.h"
#include "intelligence/ai_service.h"
#include "zero/log.h"
#include "zero/string.h"

#define GC_WIN_WIDTH  620
#define GC_WIN_HEIGHT 420

typedef enum {
    GC_TAB_PROFILES = 0,
    GC_TAB_SCHEDULER,
    GC_TAB_GRAPHICS,
    GC_TAB_AUDIO,
    GC_TAB_AI_SYNC,
    GC_TAB_COUNT
} gc_tab_t;

static window_t *s_gc_window = NULL;
static gc_tab_t  s_active_tab = GC_TAB_PROFILES;

static const char *s_tab_names[GC_TAB_COUNT] = {
    "Profiles", "Scheduler", "Graphics", "Audio", "AI Sync"
};

static void draw_stat_row(framebuffer_t *fb, int32_t x, int32_t y, const char *label, const char *val, uint32_t val_col)
{
    font_draw_string(fb, x, y, label, 1, 0x8A8A8Au);
    font_draw_string(fb, x + 240, y, val, 1, val_col);
}

static void render_profiles_tab(window_t *win, framebuffer_t *fb, int32_t cx, int32_t cy)
{
    gaming_telemetry_t telem;
    gaming_get_telemetry(&telem);

    /* Profile Buttons */
    int32_t btn_w = 170;
    int32_t btn_h = 44;

    /* Normal Profile Button */
    bool is_norm = (telem.current_profile == GAMING_PROFILE_NORMAL);
    draw_fill_rect(fb, cx, cy, btn_w, btn_h, is_norm ? 0x1E3A20u : 0x181820u);
    draw_rect(fb, cx, cy, btn_w, btn_h, is_norm ? 0x4CAF50u : 0x2C2C36u);
    font_draw_string(fb, cx + 55, cy + 12, "NORMAL", 1, is_norm ? 0x4CAF50u : 0xE8E8E8u);
    font_draw_string(fb, cx + 38, cy + 28, "Balanced OS", 1, 0x8A8A8Au);

    /* Gaming Profile Button */
    bool is_game = (telem.current_profile == GAMING_PROFILE_GAMING);
    draw_fill_rect(fb, cx + 190, cy, btn_w, btn_h, is_game ? 0x3E2E10u : 0x181820u);
    draw_rect(fb, cx + 190, cy, btn_w, btn_h, is_game ? 0xC9A227u : 0x2C2C36u);
    font_draw_string(fb, cx + 245, cy + 12, "GAMING", 1, is_game ? 0xC9A227u : 0xE8E8E8u);
    font_draw_string(fb, cx + 215, cy + 28, "Low-Latency Boost", 1, 0x8A8A8Au);

    /* Power Saver Profile Button */
    bool is_save = (telem.current_profile == GAMING_PROFILE_POWER_SAVER);
    draw_fill_rect(fb, cx + 380, cy, btn_w, btn_h, is_save ? 0x3E1818u : 0x181820u);
    draw_rect(fb, cx + 380, cy, btn_w, btn_h, is_save ? 0xB3202Bu : 0x2C2C36u);
    font_draw_string(fb, cx + 420, cy + 12, "POWER SAVER", 1, is_save ? 0xB3202Bu : 0xE8E8E8u);
    font_draw_string(fb, cx + 415, cy + 28, "Coalesced Ticks", 1, 0x8A8A8Au);

    /* Telemetry Card */
    int32_t card_y = cy + 64;
    draw_fill_rect(fb, cx, card_y, win->width - 40, 240, 0x121218u);
    draw_rect(fb, cx, card_y, win->width - 40, 240, 0x2C2C36u);

    font_draw_string(fb, cx + 16, card_y + 16, "Active Game & Telemetry Status", 1, 0xC9A227u);

    draw_stat_row(fb, cx + 16, card_y + 45, "Active Game Process:",
                  telem.active_game_pid > 0 ? telem.active_game_title : "None (Desktop Idle)",
                  telem.active_game_pid > 0 ? 0x4CAF50u : 0x8A8A8Au);

    char pid_str[16] = "N/A";
    if (telem.active_game_pid > 0) {
        pid_str[0] = 'P'; pid_str[1] = 'I'; pid_str[2] = 'D'; pid_str[3] = ' ';
        pid_str[4] = '0' + (char)(telem.active_game_pid % 10);
        pid_str[5] = '\0';
    }
    draw_stat_row(fb, cx + 16, card_y + 70, "Process ID:", pid_str, 0xE8E8E8u);

    draw_stat_row(fb, cx + 16, card_y + 95, "Target Rendering FPS:", "60.0 FPS (16.6 ms frame budget)", 0x4CAF50u);

    draw_stat_row(fb, cx + 16, card_y + 120, "Scheduler Time-Slice:",
                  is_game ? "80 ms (Foreground) / 20 ms (Background)" :
                  (is_save ? "100 ms (Power Saving)" : "50 ms (Normal Round-Robin)"),
                  0xE8E8E8u);

    draw_stat_row(fb, cx + 16, card_y + 145, "AI Background Tasks:",
                  telem.ai_throttled ? "THROTTLED / PAUSED (Conserving CPU)" : "ACTIVE (Ready)",
                  telem.ai_throttled ? 0xC9A227u : 0x4CAF50u);

    draw_stat_row(fb, cx + 16, card_y + 170, "Hardware Acceleration:",
                  telem.hardware_accelerated ? "ACTIVE (SIMD Blitter + GOP VRAM)" : "Software",
                  0x4CAF50u);

    draw_stat_row(fb, cx + 16, card_y + 195, "Profile Transitions:", "Logged to /data/log/audit.log", 0x8A8A8Au);
}

static void render_scheduler_tab(window_t *win, framebuffer_t *fb, int32_t cx, int32_t cy)
{
    struct scheduler_stats s_stats;
    scheduler_get_stats(&s_stats);

    draw_fill_rect(fb, cx, cy, win->width - 40, 310, 0x121218u);
    draw_rect(fb, cx, cy, win->width - 40, 310, 0x2C2C36u);

    font_draw_string(fb, cx + 16, cy + 16, "Preemptive Scheduler & Latency Profile", 1, 0xC9A227u);

    const char *prof = (s_stats.active_profile == SCHED_PROFILE_LOW_LATENCY) ? "LOW_LATENCY (Gaming Mode)" :
                       (s_stats.active_profile == SCHED_PROFILE_POWER_SAVER ? "POWER_SAVER" : "NORMAL");
    draw_stat_row(fb, cx + 16, cy + 45, "Active Sched Profile:", prof, 0x4CAF50u);

    draw_stat_row(fb, cx + 16, cy + 70, "Ready Queue Capacity:", "128 threads (Ring Buffer)", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 95, "Timer Frequency:", "100 Hz (10 ms APIC Periodic Tick)", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 120, "Foreground Boost Policy:", "Priority dequeue with 4x anti-starvation cap", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 145, "Starvation Defense:", "Guaranteed 20 ms slice for kernel daemons", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 170, "Idle CPU State:", "sti; hlt (Zero CPU burn when idle)", 0x8A8A8Au);
    draw_stat_row(fb, cx + 16, cy + 195, "Address Space Switch:", "CR3 hardware page table reload", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 220, "TSS Kernel Stack:", "Dynamic RSP0 update per user transition", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 245, "APIC Preemption:", "ENABLED (Vector 0x20 ISR)", 0x4CAF50u);
}

static void render_graphics_tab(window_t *win, framebuffer_t *fb, int32_t cx, int32_t cy)
{
    gpu_telemetry_t g_telem;
    gpu_get_telemetry(&g_telem);
    const gpu_device_t *g_dev = gpu_get_device();

    draw_fill_rect(fb, cx, cy, win->width - 40, 310, 0x121218u);
    draw_rect(fb, cx, cy, win->width - 40, 310, 0x2C2C36u);

    font_draw_string(fb, cx + 16, cy + 16, "Graphics Hardware Abstraction & Acceleration", 1, 0xC9A227u);

    draw_stat_row(fb, cx + 16, cy + 45, "Display Hardware:", g_dev ? g_dev->name : "Generic GOP", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 70, "Active Renderer:", g_telem.active_renderer, 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 95, "Double Buffering:", "Tear-free back buffer in System RAM", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 120, "Damage Region Tracking:", "Active dirty-rect bounding box blit", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 145, "Pixel Format:", "32-bpp BGRA / Linear MMIO", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 170, "VSync Synchronization:", "Software timer boundary (60 Hz target)", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 195, "Hardware Cursor:", "16x16 vector arrow with bg save/restore", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 220, "2D Primitives:", "Bresenham lines, rects, circles, alpha blend", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 245, "Universal Fallback:", "Guaranteed software rasterizer active", 0x4CAF50u);
}

static void render_audio_tab(window_t *win, framebuffer_t *fb, int32_t cx, int32_t cy)
{
    audio_telemetry_t a_telem;
    audio_get_telemetry(&a_telem);

    draw_fill_rect(fb, cx, cy, win->width - 40, 310, 0x121218u);
    draw_rect(fb, cx, cy, win->width - 40, 310, 0x2C2C36u);

    font_draw_string(fb, cx + 16, cy + 16, "Audio Device & Low-Latency Input Status", 1, 0xC9A227u);

    draw_stat_row(fb, cx + 16, cy + 45, "Audio Device:", a_telem.active_device_name, 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 70, "Sample Format:", "16-bit Signed PCM Stereo (48000 Hz)", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 95, "Buffer Capacity:", "8192 bytes circular ring buffer", 0xE8E8E8u);
    draw_stat_row(fb, cx + 16, cy + 120, "Volume Level:", "80% (Safe Master Output)", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 145, "Mute Status:", a_telem.is_muted ? "MUTED" : "UNMUTED", a_telem.is_muted ? 0xB3202Bu : 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 170, "PS/2 Keyboard:", "IRQ 1, Scancode Set 1, non-blocking queue", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 195, "PS/2 Mouse:", "IRQ 12, 3-byte packets, 9-bit deltas", 0x4CAF50u);
    draw_stat_row(fb, cx + 16, cy + 220, "Input Latency:", "< 8 ms interrupt-to-event dispatch", 0x4CAF50u);
}

static void render_ai_sync_tab(window_t *win, framebuffer_t *fb, int32_t cx, int32_t cy)
{
    bool ai_throttled = ai_service_is_gaming_mode();

    draw_fill_rect(fb, cx, cy, win->width - 40, 310, 0x121218u);
    draw_rect(fb, cx, cy, win->width - 40, 310, 0x2C2C36u);

    font_draw_string(fb, cx + 16, cy + 16, "ZERO Intelligence & Game Resource Synchronization", 1, 0xC9A227u);

    draw_stat_row(fb, cx + 16, cy + 45, "AI Resource State:",
                  ai_throttled ? "THROTTLED (Gaming Priority Active)" : "NORMAL (Full Availability)",
                  ai_throttled ? 0xC9A227u : 0x4CAF50u);

    draw_stat_row(fb, cx + 16, cy + 70, "Background Inference:",
                  ai_throttled ? "PAUSED (0% CPU background overhead)" : "ALLOWED (Round-robin)",
                  ai_throttled ? 0x4CAF50u : 0xE8E8E8u);

    draw_stat_row(fb, cx + 16, cy + 95, "Model Memory Policy:",
                  "Single-Active-Model invariant (Max 256 MB)", 0x4CAF50u);

    draw_stat_row(fb, cx + 16, cy + 120, "Memory Pressure Action:",
                  "Optional model weights evicted if memory tight", 0x4CAF50u);

    draw_stat_row(fb, cx + 16, cy + 145, "Assistant Security:",
                  "Unprivileged advisory daemon (ZERO Security enforced)", 0x4CAF50u);

    draw_stat_row(fb, cx + 16, cy + 170, "Permission Broker:",
                  "User Approval Gate remains strictly active", 0x4CAF50u);
}

static void gc_paint(window_t *win, framebuffer_t *fb)
{
    /* Window Background */
    draw_fill_rect(fb, win->x, win->y, win->width, win->height, 0x0E0E12u);

    /* Header Accent */
    draw_fill_rect(fb, win->x, win->y, win->width, 40, 0x14141Cu);
    draw_line(fb, win->x, win->y + 40, win->x + win->width, win->y + 40, 0xC9A227u);

    /* Tab Buttons */
    int32_t tab_w = 95;
    for (int32_t i = 0; i < GC_TAB_COUNT; i++) {
        int32_t tx = win->x + 16 + i * (tab_w + 6);
        bool active = (s_active_tab == (gc_tab_t)i);
        draw_fill_rect(fb, tx, win->y + 8, tab_w, 26, active ? 0xC9A227u : 0x1E1E28u);
        font_draw_string(fb, tx + 14, win->y + 14, s_tab_names[i], 1, active ? 0x0A0A0Au : 0xE8E8E8u);
    }

    int32_t content_x = win->x + 20;
    int32_t content_y = win->y + 56;

    switch (s_active_tab) {
        case GC_TAB_PROFILES:  render_profiles_tab(win, fb, content_x, content_y); break;
        case GC_TAB_SCHEDULER: render_scheduler_tab(win, fb, content_x, content_y); break;
        case GC_TAB_GRAPHICS:  render_graphics_tab(win, fb, content_x, content_y); break;
        case GC_TAB_AUDIO:     render_audio_tab(win, fb, content_x, content_y); break;
        case GC_TAB_AI_SYNC:   render_ai_sync_tab(win, fb, content_x, content_y); break;
        default: break;
    }
}

static void gc_on_click(window_t *win, int32_t rx, int32_t ry, uint8_t buttons)
{
    (void)buttons;

    /* Check tab clicks */
    if (ry >= 8 && ry <= 34) {
        int32_t tab_w = 95;
        for (int32_t i = 0; i < GC_TAB_COUNT; i++) {
            int32_t tx = 16 + i * (tab_w + 6);
            if (rx >= tx && rx <= tx + tab_w) {
                s_active_tab = (gc_tab_t)i;
                window_invalidate(win);
                return;
            }
        }
    }

    /* Check profile button clicks on Profiles tab */
    if (s_active_tab == GC_TAB_PROFILES) {
        int32_t cy = 56;
        int32_t btn_w = 170;
        int32_t btn_h = 44;

        if (ry >= cy && ry <= cy + btn_h) {
            /* Normal button */
            if (rx >= 20 && rx <= 20 + btn_w) {
                gaming_set_profile(GAMING_PROFILE_NORMAL);
                window_invalidate(win);
            }
            /* Gaming button */
            else if (rx >= 210 && rx <= 210 + btn_w) {
                gaming_set_profile(GAMING_PROFILE_GAMING);
                window_invalidate(win);
            }
            /* Power Saver button */
            else if (rx >= 400 && rx <= 400 + btn_w) {
                gaming_set_profile(GAMING_PROFILE_POWER_SAVER);
                window_invalidate(win);
            }
        }
    }
}

void gaming_control_init(void)
{
    s_active_tab = GC_TAB_PROFILES;
}

window_t *gaming_control_launch(void)
{
    if (s_gc_window) {
        window_focus(s_gc_window);
        return s_gc_window;
    }

    s_gc_window = wm_create_window("ZERO Gaming Control Center", 80, 50,
                                GC_WIN_WIDTH, GC_WIN_HEIGHT,
                                WINDOW_FLAG_RESIZABLE);
    if (!s_gc_window) return NULL;

    s_gc_window->on_paint = gc_paint;
    s_gc_window->on_mouse = gc_on_click;

    KLOG_INFO("gc", "launched Gaming Control Center GUI");
    return s_gc_window;
}

bool gaming_control_is_open(void)
{
    return (s_gc_window != NULL);
}
