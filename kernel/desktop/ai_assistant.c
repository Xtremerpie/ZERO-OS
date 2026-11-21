/* ZERO KERNEL - Native ZERO AI Assistant Application Implementation (Milestone 8.0.0 ZERO Intelligence).
 *
 * Implements a 6-view native graphical AI desktop application adhering to
 * ZERO OS Black / Graphite / Gold / Crimson visual aesthetic.
 */
#include "desktop/ai_assistant.h"
#include "desktop/window.h"
#include "desktop/compositor.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "intelligence/ai_service.h"
#include "intelligence/ai_daemon.h"
#include "intelligence/ai_provider.h"
#include "intelligence/ai_model.h"
#include "intelligence/ai_engine.h"
#include "intelligence/ai_automation.h"
#include "intelligence/ai_memory.h"
#include "zero/string.h"
#include <stdio.h>

#define AI_TAB_COUNT 6

static window_t *s_ai_window = NULL;
static uint32_t  s_active_tab = 0;
static uint32_t  s_active_session_id = 1;
static char      s_input_buffer[128];
static uint32_t  s_input_len = 0;

static const char *const s_tab_titles[AI_TAB_COUNT] = {
    "Chat",
    "Models",
    "Automation",
    "Memory",
    "Settings",
    "Service"
};

static void draw_badge(framebuffer_t *fb, int32_t x, int32_t y, const char *text, uint32_t bg, uint32_t fg)
{
    int32_t len = (int32_t)strlen(text);
    int32_t w = len * (int32_t)FONT_BASE_WIDTH + 10;
    int32_t h = (int32_t)FONT_BASE_HEIGHT + 6;
    draw_fill_rect(fb, x, y, w, h, bg);
    draw_rect(fb, x, y, w, h, fg);
    font_draw_string(fb, x + 5, y + 3, text, 1, fg);
}

/* Tab 0: Chat View */
static void draw_view_chat(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    /* Sub-header status bar */
    draw_fill_rect(fb, cx + 10, cy + 8, cw - 20, 26, 0x14141Cu);
    draw_rect(fb, cx + 10, cy + 8, cw - 20, 26, COLOR_ZERO_BORDER);

    const ai_model_desc_t *m = ai_model_get_active();
    char stat_str[80];
    snprintf(stat_str, sizeof(stat_str), "Model: %s | Provider: %s",
             m ? m->name : "None",
             ai_provider_get_active() ? ai_provider_get_active()->name : "None");
    font_draw_string(fb, cx + 18, cy + 14, stat_str, 1, COLOR_ZERO_GOLD);

    ai_daemon_status_t dstat;
    ai_daemon_get_status(&dstat);
    const char *state_txt = "IDLE";
    uint32_t state_col = COLOR_ZERO_GOLD;
    if (dstat.state == AI_DAEMON_BUSY_INFERENCE) { state_txt = "THINKING"; state_col = COLOR_ZERO_INFO; }
    else if (dstat.state == AI_DAEMON_AWAITING_APPROVAL) { state_txt = "AWAITING APPROVAL"; state_col = COLOR_ZERO_WARNING; }
    else if (dstat.state == AI_DAEMON_STOPPED) { state_txt = "STOPPED"; state_col = COLOR_ZERO_CRIMSON; }
    draw_badge(fb, cx + cw - 150, cy + 10, state_txt, 0x102010u, state_col);

    /* Message conversation area */
    int32_t msg_area_y = cy + 40;
    int32_t msg_area_h = ch - 85;
    draw_fill_rect(fb, cx + 10, msg_area_y, cw - 20, msg_area_h, 0x0C0C10u);
    draw_rect(fb, cx + 10, msg_area_y, cw - 20, msg_area_h, COLOR_ZERO_BORDER);

    ai_session_t *s = ai_engine_get_session(s_active_session_id);
    if (!s || s->msg_count == 0) {
        font_draw_string(fb, cx + 25, msg_area_y + 20, "Type a message below or ask to launch an app (e.g. 'open calc')", 1, COLOR_ZERO_MUTED);
    } else {
        int32_t y = msg_area_y + 12;
        uint32_t start_idx = (s->msg_count > 6) ? (s->msg_count - 6) : 0;
        for (uint32_t i = start_idx; i < s->msg_count && y < msg_area_y + msg_area_h - 30; i++) {
            const ai_message_t *msg = &s->messages[i];
            if (msg->role == AI_MSG_ROLE_USER) {
                font_draw_string(fb, cx + 20, y, "YOU:", 1, COLOR_ZERO_GOLD);
                font_draw_string(fb, cx + 65, y, msg->text, 1, COLOR_ZERO_WHITE);
            } else if (msg->role == AI_MSG_ROLE_ASSISTANT) {
                font_draw_string(fb, cx + 20, y, "ZERO:", 1, COLOR_ZERO_INFO);
                font_draw_string(fb, cx + 70, y, msg->text, 1, COLOR_ZERO_WHITE);
            } else {
                font_draw_string(fb, cx + 20, y, "SYS:", 1, COLOR_ZERO_MUTED);
                font_draw_string(fb, cx + 65, y, msg->text, 1, COLOR_ZERO_MUTED);
            }
            y += 24;
        }
    }

    /* Prompt Input Bar */
    int32_t input_y = cy + ch - 38;
    draw_fill_rect(fb, cx + 10, input_y, cw - 100, 28, 0x161620u);
    draw_rect(fb, cx + 10, input_y, cw - 100, 28, COLOR_ZERO_BORDER_FOCUS);

    char display_input[140];
    snprintf(display_input, sizeof(display_input), "%s_", s_input_buffer);
    font_draw_string(fb, cx + 18, input_y + 8, display_input, 1, COLOR_ZERO_WHITE);

    /* Send Button */
    draw_fill_rect(fb, cx + cw - 80, input_y, 70, 28, 0x222230u);
    draw_rect(fb, cx + cw - 80, input_y, 70, 28, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + cw - 65, input_y + 8, "Send", 1, COLOR_ZERO_GOLD);
}

/* Tab 1: Models View */
static void draw_view_models(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "LOCAL & CONNECTED AI MODELS (SINGLE ACTIVE POLICY)", 1, COLOR_ZERO_GOLD);

    /* Table Header */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, cy + 34, "ID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 50, cy + 34, "MODEL NAME", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 180, cy + 34, "ARCHITECTURE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 320, cy + 34, "PARAMS", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 390, cy + 34, "QUANT", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 460, cy + 34, "RAM EST", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 530, cy + 34, "STATUS", 1, COLOR_ZERO_MUTED);

    ai_model_desc_t models[AI_MAX_MODELS];
    uint32_t count = ai_model_list(models, AI_MAX_MODELS);
    int32_t row_y = cy + 56;

    for (uint32_t i = 0; i < count; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", models[i].id);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_TEXT);

        font_draw_string(fb, cx + 50, row_y, models[i].name, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 180, row_y, models[i].architecture, 1, COLOR_ZERO_MUTED);

        if (models[i].param_count_m > 0) {
            snprintf(buf, sizeof(buf), "%uM", models[i].param_count_m);
        } else {
            strcpy(buf, "N/A");
        }
        font_draw_string(fb, cx + 320, row_y, buf, 1, COLOR_ZERO_TEXT);
        font_draw_string(fb, cx + 390, row_y, models[i].quantization, 1, COLOR_ZERO_TEXT);

        snprintf(buf, sizeof(buf), "%u MB", models[i].ram_required_mb);
        font_draw_string(fb, cx + 460, row_y, buf, 1, COLOR_ZERO_TEXT);

        if (models[i].state == AI_MODEL_STATE_LOADED) {
            draw_badge(fb, cx + 530, row_y - 2, "LOADED", 0x102010u, COLOR_ZERO_GOLD);
        } else {
            font_draw_string(fb, cx + 530, row_y, "[Click Load]", 1, COLOR_ZERO_MUTED);
        }

        row_y += 26;
    }
}

/* Tab 2: Automation & Approvals View */
static void draw_view_automation(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "PENDING AUTOMATION ACTIONS (PERMISSION-GATED)", 1, COLOR_ZERO_GOLD);

    ai_action_t pending[4];
    uint32_t pcount = ai_automation_get_pending(pending, 4);

    if (pcount == 0) {
        draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 40, 0x14141Cu);
        draw_rect(fb, cx + 12, cy + 30, cw - 24, 40, COLOR_ZERO_BORDER);
        font_draw_string(fb, cx + 25, cy + 42, "No pending automation requests. All operations authorized or completed.", 1, COLOR_ZERO_MUTED);
    } else {
        int32_t py = cy + 30;
        for (uint32_t i = 0; i < pcount; i++) {
            draw_fill_rect(fb, cx + 12, py, cw - 24, 46, 0x181822u);
            draw_rect(fb, cx + 12, py, cw - 24, 46, COLOR_ZERO_GOLD);

            char buf[80];
            snprintf(buf, sizeof(buf), "Action #%u: %s (Target: %s)", pending[i].id, pending[i].description, pending[i].target);
            font_draw_string(fb, cx + 20, py + 8, buf, 1, COLOR_ZERO_WHITE);
            font_draw_string(fb, cx + 20, py + 26, "Requires explicit confirmation before execution.", 1, COLOR_ZERO_WARNING);

            /* Approve Button */
            draw_fill_rect(fb, cx + cw - 180, py + 8, 70, 26, 0x102810u);
            draw_rect(fb, cx + cw - 180, py + 8, 70, 26, COLOR_ZERO_GOLD);
            font_draw_string(fb, cx + cw - 165, py + 14, "Approve", 1, COLOR_ZERO_GOLD);

            /* Reject Button */
            draw_fill_rect(fb, cx + cw - 100, py + 8, 70, 26, 0x2A1010u);
            draw_rect(fb, cx + cw - 100, py + 8, 70, 26, COLOR_ZERO_CRIMSON);
            font_draw_string(fb, cx + cw - 88, py + 14, "Reject", 1, COLOR_ZERO_CRIMSON);

            py += 54;
        }
    }

    /* Action Execution History */
    int32_t hy = cy + 150;
    font_draw_string(fb, cx + 12, hy, "ACTION AUDIT TRAIL & EXECUTION OUTCOMES", 1, COLOR_ZERO_GOLD);

    draw_fill_rect(fb, cx + 12, hy + 20, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, hy + 24, "ID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 60, hy + 24, "TARGET", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 200, hy + 24, "DESCRIPTION", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 420, hy + 24, "STATUS", 1, COLOR_ZERO_MUTED);

    ai_action_t history[8];
    uint32_t hcount = ai_automation_get_history(history, 8);
    int32_t row_y = hy + 44;

    for (uint32_t i = 0; i < hcount; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "#%u", history[i].id);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_MUTED);

        font_draw_string(fb, cx + 60, row_y, history[i].target, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 200, row_y, history[i].description, 1, COLOR_ZERO_MUTED);

        const char *sstr = "UNKNOWN";
        uint32_t scol = COLOR_ZERO_MUTED;
        if (history[i].status == AI_ACTION_STATUS_EXECUTED) { sstr = "EXECUTED"; scol = COLOR_ZERO_GOLD; }
        else if (history[i].status == AI_ACTION_STATUS_REJECTED) { sstr = "REJECTED"; scol = COLOR_ZERO_CRIMSON; }
        else if (history[i].status == AI_ACTION_STATUS_SECURITY_DENIED) { sstr = "SEC DENIED"; scol = COLOR_ZERO_CRIMSON; }
        font_draw_string(fb, cx + 420, row_y, sstr, 1, scol);

        row_y += 22;
    }
}

/* Tab 3: Memory View */
static void draw_view_memory(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "CONVERSATION MEMORY & USER PREFERENCES (/data/ai)", 1, COLOR_ZERO_GOLD);

    ai_user_prefs_t prefs;
    ai_memory_load_prefs(&prefs);

    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 70, 0x14141Cu);
    draw_rect(fb, cx + 12, cy + 30, cw - 24, 70, COLOR_ZERO_BORDER);

    char buf[128];
    snprintf(buf, sizeof(buf), "Default Model:           %s", prefs.default_model);
    font_draw_string(fb, cx + 24, cy + 40, buf, 1, COLOR_ZERO_WHITE);
    snprintf(buf, sizeof(buf), "Max Context Tokens:      %u", prefs.max_context_tokens);
    font_draw_string(fb, cx + 24, cy + 58, buf, 1, COLOR_ZERO_WHITE);
    snprintf(buf, sizeof(buf), "Toast Notifications:     %s", prefs.notifications_enabled ? "ENABLED" : "DISABLED");
    font_draw_string(fb, cx + 24, cy + 76, buf, 1, COLOR_ZERO_WHITE);

    /* Clear Memory Button */
    draw_fill_rect(fb, cx + cw - 180, cy + 45, 150, 30, 0x2A1010u);
    draw_rect(fb, cx + cw - 180, cy + 45, 150, 30, COLOR_ZERO_CRIMSON);
    font_draw_string(fb, cx + cw - 165, cy + 54, "[Clear History]", 1, COLOR_ZERO_CRIMSON);

    /* Saved Sessions Table */
    int32_t sy = cy + 115;
    font_draw_string(fb, cx + 12, sy, "ACTIVE CONVERSATION SESSIONS", 1, COLOR_ZERO_GOLD);

    draw_fill_rect(fb, cx + 12, sy + 20, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, sy + 24, "ID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 60, sy + 24, "SESSION TITLE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 220, sy + 24, "MESSAGES", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 320, sy + 24, "LAST ACTIVE", 1, COLOR_ZERO_MUTED);

    ai_session_t sessions[AI_MAX_SESSIONS];
    uint32_t scount = ai_engine_list_sessions(sessions, AI_MAX_SESSIONS);
    int32_t row_y = sy + 44;

    for (uint32_t i = 0; i < scount; i++) {
        snprintf(buf, sizeof(buf), "#%u", sessions[i].id);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_MUTED);
        font_draw_string(fb, cx + 60, row_y, sessions[i].title, 1, COLOR_ZERO_WHITE);

        snprintf(buf, sizeof(buf), "%u msgs", sessions[i].msg_count);
        font_draw_string(fb, cx + 220, row_y, buf, 1, COLOR_ZERO_TEXT);

        snprintf(buf, sizeof(buf), "%llu ms", (unsigned long long)sessions[i].last_active_ms);
        font_draw_string(fb, cx + 320, row_y, buf, 1, COLOR_ZERO_MUTED);

        row_y += 22;
    }
}

/* Tab 4: Settings View */
static void draw_view_settings(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "ZERO INTELLIGENCE RUNTIME SETTINGS", 1, COLOR_ZERO_GOLD);

    /* Provider Selection */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 70, 0x14141Cu);
    draw_rect(fb, cx + 12, cy + 30, cw - 24, 70, COLOR_ZERO_BORDER);

    ai_provider_t *p = ai_provider_get_active();
    font_draw_string(fb, cx + 24, cy + 42, "Active AI Provider:", 1, COLOR_ZERO_TEXT);
    font_draw_string(fb, cx + 170, cy + 42, p ? p->name : "None", 1, COLOR_ZERO_GOLD);

    draw_fill_rect(fb, cx + cw - 180, cy + 38, 150, 26, 0x222230u);
    draw_rect(fb, cx + cw - 180, cy + 38, 150, 26, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + cw - 170, cy + 44, "[Toggle Provider]", 1, COLOR_ZERO_GOLD);

    /* Resource Governance */
    int32_t gy = cy + 115;
    font_draw_string(fb, cx + 12, gy, "HARDWARE & RESOURCE CONSTRAINTS (CELERON J3355)", 1, COLOR_ZERO_GOLD);

    struct {
        const char *name;
        const char *desc;
        const char *val;
    } items[] = {
        { "Model Memory Ceiling", "Max allocated model weights buffer", "256 MB" },
        { "Inference Concurrency", "Simultaneous active model generation threads", "1 (Single)" },
        { "Context Token Window", "Max token retention in session memory", "1024 Tokens" },
        { "Prompt Timeout", "Max execution time before inference abort", "15000 ms" },
        { "Direct Shell Access", "Unrestricted shell/terminal execution", "DISABLED (Blocked)" },
        { "Self-Authorization", "AI self-approval of privileged actions", "FORBIDDEN (Blocked)" }
    };

    int32_t iy = gy + 25;
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        draw_fill_rect(fb, cx + 12, iy, cw - 24, 24, (i % 2 == 0) ? 0x14141Cu : 0x181822u);
        font_draw_string(fb, cx + 20, iy + 6, items[i].name, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 220, iy + 6, items[i].desc, 1, COLOR_ZERO_MUTED);
        font_draw_string(fb, cx + cw - 160, iy + 6, items[i].val, 1, COLOR_ZERO_GOLD);
        iy += 26;
    }
}

/* Tab 5: Service & Daemon Telemetry View */
static void draw_view_service(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "ZERO AI ASSISTANT DAEMON (zero-ai-daemon)", 1, COLOR_ZERO_GOLD);

    ai_daemon_status_t dstat;
    ai_daemon_get_status(&dstat);

    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 70, 0x14141Cu);
    draw_rect(fb, cx + 12, cy + 30, cw - 24, 70, COLOR_ZERO_BORDER);

    char buf[128];
    snprintf(buf, sizeof(buf), "State: %s | Uptime: %llu sec | Memory: %u KB",
             (dstat.state != AI_DAEMON_STOPPED) ? "ACTIVE" : "STOPPED",
             (unsigned long long)(dstat.uptime_ms / 1000),
             dstat.memory_used_kb);
    font_draw_string(fb, cx + 24, cy + 42, buf, 1, COLOR_ZERO_WHITE);

    /* Control buttons */
    draw_fill_rect(fb, cx + cw - 240, cy + 40, 65, 24, 0x142814u);
    draw_rect(fb, cx + cw - 240, cy + 40, 65, 24, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + cw - 228, cy + 46, "Start", 1, COLOR_ZERO_GOLD);

    draw_fill_rect(fb, cx + cw - 165, cy + 40, 65, 24, 0x2A1010u);
    draw_rect(fb, cx + cw - 165, cy + 40, 65, 24, COLOR_ZERO_CRIMSON);
    font_draw_string(fb, cx + cw - 153, cy + 46, "Stop", 1, COLOR_ZERO_CRIMSON);

    draw_fill_rect(fb, cx + cw - 90, cy + 40, 75, 24, 0x222230u);
    draw_rect(fb, cx + cw - 90, cy + 40, 75, 24, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + cw - 80, cy + 46, "Restart", 1, COLOR_ZERO_GOLD);

    /* Live Daemon Event Logs */
    int32_t ly = cy + 115;
    font_draw_string(fb, cx + 12, ly, "LIVE DAEMON TELEMETRY & EVENT LOGS", 1, COLOR_ZERO_GOLD);

    draw_fill_rect(fb, cx + 12, ly + 20, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, ly + 24, "TIME", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 100, ly + 24, "EVENT LOG MESSAGE", 1, COLOR_ZERO_MUTED);

    ai_daemon_log_t logs[8];
    uint32_t lcount = ai_daemon_get_logs(logs, 8);
    int32_t row_y = ly + 44;

    for (uint32_t i = 0; i < lcount; i++) {
        snprintf(buf, sizeof(buf), "[%llu ms]", (unsigned long long)logs[i].timestamp_ms);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_MUTED);

        font_draw_string(fb, cx + 100, row_y, logs[i].message, 1, logs[i].is_error ? COLOR_ZERO_CRIMSON : COLOR_ZERO_WHITE);
        row_y += 22;
    }
}

static void ai_on_paint(window_t *win, framebuffer_t *fb)
{
    if (!win || !fb) return;

    int32_t cx = win->content_x;
    int32_t cy = win->content_y;
    int32_t cw = win->content_w;
    int32_t ch = win->content_h;

    draw_fill_rect(fb, cx, cy, cw, ch, COLOR_ZERO_BLACK);

    /* Render Tab Header */
    int32_t tab_w = (cw - 16) / AI_TAB_COUNT;
    int32_t tab_h = 28;

    draw_fill_rect(fb, cx, cy, cw, tab_h, 0x121218u);
    draw_line(fb, cx, cy + tab_h, cx + cw, cy + tab_h, COLOR_ZERO_BORDER);

    for (uint32_t i = 0; i < AI_TAB_COUNT; i++) {
        int32_t tx = cx + 8 + (int32_t)i * tab_w;
        bool is_active = (i == s_active_tab);

        if (is_active) {
            draw_fill_rect(fb, tx, cy + 2, tab_w - 4, tab_h - 2, 0x22222Du);
            draw_line(fb, tx, cy + tab_h - 1, tx + tab_w - 4, cy + tab_h - 1, COLOR_ZERO_GOLD);
            font_draw_string(fb, tx + 6, cy + 8, s_tab_titles[i], 1, COLOR_ZERO_GOLD);
        } else {
            font_draw_string(fb, tx + 6, cy + 8, s_tab_titles[i], 1, COLOR_ZERO_MUTED);
        }
    }

    int32_t vy = cy + tab_h;
    int32_t vh = ch - tab_h;

    switch (s_active_tab) {
    case 0: draw_view_chat(fb, cx, vy, cw, vh); break;
    case 1: draw_view_models(fb, cx, vy, cw, vh); break;
    case 2: draw_view_automation(fb, cx, vy, cw, vh); break;
    case 3: draw_view_memory(fb, cx, vy, cw, vh); break;
    case 4: draw_view_settings(fb, cx, vy, cw, vh); break;
    case 5: draw_view_service(fb, cx, vy, cw, vh); break;
    default: break;
    }
}

static void ai_on_mouse(window_t *win, int32_t mx, int32_t my, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1)) return;

    int32_t tab_w = (AI_ASSISTANT_WIDTH - 16) / AI_TAB_COUNT;
    int32_t tab_h = 28;

    /* Tab bar clicks */
    if (my >= 0 && my < tab_h) {
        for (uint32_t i = 0; i < AI_TAB_COUNT; i++) {
            int32_t tx = 8 + (int32_t)i * tab_w;
            if (mx >= tx && mx < tx + tab_w) {
                s_active_tab = i;
                compositor_mark_dirty();
                return;
            }
        }
    }

    /* Tab 0: Send Button Click */
    if (s_active_tab == 0 && my >= AI_ASSISTANT_HEIGHT - 38 && my < AI_ASSISTANT_HEIGHT - 10) {
        if (mx >= AI_ASSISTANT_WIDTH - 80 && mx < AI_ASSISTANT_WIDTH - 10) {
            if (s_input_len > 0) {
                char resp[512];
                ai_daemon_submit_prompt(s_active_session_id, s_input_buffer, resp, sizeof(resp));
                s_input_buffer[0] = '\0';
                s_input_len = 0;
                compositor_mark_dirty();
                return;
            }
        }
    }

    /* Tab 1: Model Load Clicks */
    if (s_active_tab == 1 && my >= 56 && my < 160) {
        uint32_t idx = (uint32_t)((my - 56) / 26);
        if (idx < ai_model_get_count()) {
            ai_model_load(idx + 1);
            compositor_mark_dirty();
            return;
        }
    }

    /* Tab 2: Approve / Reject Automation Clicks */
    if (s_active_tab == 2 && my >= 30 + tab_h && my < 120 + tab_h) {
        ai_action_t pending[4];
        uint32_t pcount = ai_automation_get_pending(pending, 4);
        if (pcount > 0) {
            /* Check Approve button [x: cw-180 .. cw-110] */
            if (mx >= AI_ASSISTANT_WIDTH - 180 && mx < AI_ASSISTANT_WIDTH - 110) {
                ai_automation_approve(pending[0].id);
                char res[256];
                ai_automation_execute(pending[0].id, res, sizeof(res));
                compositor_mark_dirty();
                return;
            }
            /* Check Reject button [x: cw-100 .. cw-30] */
            if (mx >= AI_ASSISTANT_WIDTH - 100 && mx < AI_ASSISTANT_WIDTH - 30) {
                ai_automation_reject(pending[0].id);
                compositor_mark_dirty();
                return;
            }
        }
    }

    /* Tab 3: Clear History Click */
    if (s_active_tab == 3 && my >= 45 + tab_h && my < 75 + tab_h) {
        if (mx >= AI_ASSISTANT_WIDTH - 180 && mx < AI_ASSISTANT_WIDTH - 30) {
            ai_memory_clear_all_history();
            ai_engine_clear_session(s_active_session_id);
            compositor_mark_dirty();
            return;
        }
    }

    /* Tab 4: Toggle Provider Click */
    if (s_active_tab == 4 && my >= 38 + tab_h && my < 64 + tab_h) {
        if (mx >= AI_ASSISTANT_WIDTH - 180 && mx < AI_ASSISTANT_WIDTH - 30) {
            ai_provider_t *cur = ai_provider_get_active();
            uint32_t next_id = (cur && cur->id == 1) ? 2 : 1;
            ai_provider_set_active(next_id);
            compositor_mark_dirty();
            return;
        }
    }

    /* Tab 5: Daemon Start / Stop / Restart Clicks */
    if (s_active_tab == 5 && my >= 40 + tab_h && my < 64 + tab_h) {
        if (mx >= AI_ASSISTANT_WIDTH - 240 && mx < AI_ASSISTANT_WIDTH - 175) {
            ai_daemon_start();
            compositor_mark_dirty();
            return;
        }
        if (mx >= AI_ASSISTANT_WIDTH - 165 && mx < AI_ASSISTANT_WIDTH - 100) {
            ai_daemon_stop();
            compositor_mark_dirty();
            return;
        }
        if (mx >= AI_ASSISTANT_WIDTH - 90 && mx < AI_ASSISTANT_WIDTH - 15) {
            ai_daemon_restart();
            compositor_mark_dirty();
            return;
        }
    }
}

static void ai_on_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)win;
    (void)scancode;

    if (key == '\t') {
        s_active_tab = (s_active_tab + 1) % AI_TAB_COUNT;
        compositor_mark_dirty();
        return;
    }

    if (s_active_tab == 0) {
        if (key == '\n' || key == '\r') {
            if (s_input_len > 0) {
                char resp[512];
                ai_daemon_submit_prompt(s_active_session_id, s_input_buffer, resp, sizeof(resp));
                s_input_buffer[0] = '\0';
                s_input_len = 0;
                compositor_mark_dirty();
            }
            return;
        }
        if (key == '\b' || key == 0x7F) {
            if (s_input_len > 0) {
                s_input_buffer[--s_input_len] = '\0';
                compositor_mark_dirty();
            }
            return;
        }
        if (key >= 0x20 && key <= 0x7E && s_input_len < sizeof(s_input_buffer) - 2) {
            s_input_buffer[s_input_len++] = (char)key;
            s_input_buffer[s_input_len] = '\0';
            compositor_mark_dirty();
            return;
        }
    }
}

static void ai_on_close(window_t *win)
{
    (void)win;
    s_ai_window = NULL;
}

void ai_assistant_init(void)
{
    s_ai_window = NULL;
    s_active_tab = 0;
    s_active_session_id = 1;
    s_input_buffer[0] = '\0';
    s_input_len = 0;
}

window_t *ai_assistant_launch(void)
{
    if (s_ai_window) {
        wm_restore(s_ai_window);
        wm_set_focus(s_ai_window);
        return s_ai_window;
    }

    s_ai_window = wm_create_window(
        100, 70, AI_ASSISTANT_WIDTH, AI_ASSISTANT_HEIGHT,
        "ZERO AI Assistant",
        WINDOW_FLAG_RESIZABLE
    );

    if (s_ai_window) {
        s_ai_window->icon_glyph = GLYPH_ICON_TERMINAL;
        s_ai_window->on_paint = ai_on_paint;
        s_ai_window->on_mouse = ai_on_mouse;
        s_ai_window->on_key = ai_on_key;
        s_ai_window->on_close = ai_on_close;
        wm_set_focus(s_ai_window);
    }

    return s_ai_window;
}

bool ai_assistant_is_open(void)
{
    return (s_ai_window != NULL);
}

void ai_assistant_refresh(void)
{
    if (s_ai_window) {
        compositor_mark_dirty();
    }
}
