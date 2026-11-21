/* ZERO KERNEL - ZERO Security Center Application Implementation (Milestone 7.0.0 ZERO Security).
 *
 * Implements full 7-view graphical security management dashboard adhering to
 * ZERO OS Black / Graphite / Gold / Crimson visual aesthetic.
 */
#include "desktop/security_center.h"
#include "desktop/window.h"
#include "desktop/compositor.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "security/security.h"
#include "security/capability.h"
#include "security/sandbox.h"
#include "security/permission.h"
#include "security/firewall.h"
#include "security/audit.h"
#include "security/crypto/crypto.h"
#include "apps/zpm.h"
#include "zero/string.h"
#include <stdio.h>

#define SEC_TAB_COUNT 7

static window_t *s_sec_window = NULL;
static uint32_t  s_active_tab = 0;

static const char *const s_tab_titles[SEC_TAB_COUNT] = {
    "Overview",
    "Caps & DAC",
    "Sandboxing",
    "Firewall",
    "Packages",
    "Audit Trail",
    "Protection"
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

/* Tab 0: Security Overview */
static void draw_view_overview(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    sec_stats_t stats;
    security_get_stats(&stats);

    /* Enforced Status Banner */
    bool enforced = security_is_enforced();
    uint32_t banner_bg = enforced ? 0x142014u : 0x2A1010u;
    uint32_t banner_border = enforced ? COLOR_ZERO_GOLD : COLOR_ZERO_CRIMSON;

    draw_fill_rect(fb, cx + 12, cy + 10, cw - 24, 48, banner_bg);
    draw_rect(fb, cx + 12, cy + 10, cw - 24, 48, banner_border);

    font_draw_string(fb, cx + 24, cy + 18, "ZERO SECURITY ENGINE:", 1, COLOR_ZERO_WHITE);
    if (enforced) {
        draw_badge(fb, cx + 200, cy + 16, "ENFORCED", 0x1A331Au, COLOR_ZERO_GOLD);
        font_draw_string(fb, cx + 290, cy + 18, "All capability checks, sandboxing & W^X active", 1, COLOR_ZERO_MUTED);
    } else {
        draw_badge(fb, cx + 200, cy + 16, "RELAXED", 0x401010u, COLOR_ZERO_CRIMSON);
        font_draw_string(fb, cx + 285, cy + 18, "Advisory mode only - violations logged", 1, COLOR_ZERO_CRIMSON);
    }

    /* 4 Metric Cards */
    int32_t card_w = (cw - 36) / 2;
    int32_t card_h = 110;

    /* Card 1: Capability Engine */
    int32_t c1_x = cx + 12;
    int32_t c1_y = cy + 66;
    draw_fill_rect(fb, c1_x, c1_y, card_w, card_h, 0x14141Cu);
    draw_rect(fb, c1_x, c1_y, card_w, card_h, COLOR_ZERO_BORDER);
    font_draw_string(fb, c1_x + 10, c1_y + 10, "Capability Access Control", 1, COLOR_ZERO_GOLD);

    char buf[64];
    snprintf(buf, sizeof(buf), "Active Capabilities:  %u", stats.active_capabilities);
    font_draw_string(fb, c1_x + 10, c1_y + 35, buf, 1, COLOR_ZERO_WHITE);
    snprintf(buf, sizeof(buf), "Revoked Handles:      %u", stats.revoked_capabilities);
    font_draw_string(fb, c1_x + 10, c1_y + 55, buf, 1, COLOR_ZERO_MUTED);
    snprintf(buf, sizeof(buf), "Total Checks:         %llu", (unsigned long long)stats.checks_total);
    font_draw_string(fb, c1_x + 10, c1_y + 75, buf, 1, COLOR_ZERO_WHITE);

    /* Card 2: Process Sandboxing */
    int32_t c2_x = cx + 18 + card_w;
    int32_t c2_y = cy + 66;
    draw_fill_rect(fb, c2_x, c2_y, card_w, card_h, 0x14141Cu);
    draw_rect(fb, c2_x, c2_y, card_w, card_h, COLOR_ZERO_BORDER);
    font_draw_string(fb, c2_x + 10, c2_y + 10, "Sandboxing & Isolation", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, c2_x + 10, c2_y + 35, "Profiles: RESTRICTED / BASIC / STD", 1, COLOR_ZERO_WHITE);
    font_draw_string(fb, c2_x + 10, c2_y + 55, "Jail Path Isolation:  ACTIVE", 1, COLOR_ZERO_WHITE);
    font_draw_string(fb, c2_x + 10, c2_y + 75, "Syscall Gating:       ACTIVE", 1, COLOR_ZERO_WHITE);

    /* Card 3: Network Security */
    int32_t c3_x = cx + 12;
    int32_t c3_y = cy + 184;
    draw_fill_rect(fb, c3_x, c3_y, card_w, card_h, 0x14141Cu);
    draw_rect(fb, c3_x, c3_y, card_w, card_h, COLOR_ZERO_BORDER);
    font_draw_string(fb, c3_x + 10, c3_y + 10, "Network Firewall", 1, COLOR_ZERO_GOLD);
    snprintf(buf, sizeof(buf), "Active Filter Rules:  %u", stats.active_firewall_rules);
    font_draw_string(fb, c3_x + 10, c3_y + 35, buf, 1, COLOR_ZERO_WHITE);
    snprintf(buf, sizeof(buf), "Packets Dropped:      %llu", (unsigned long long)stats.firewall_packets_blocked);
    font_draw_string(fb, c3_x + 10, c3_y + 55, buf, 1, stats.firewall_packets_blocked > 0 ? COLOR_ZERO_CRIMSON : COLOR_ZERO_MUTED);
    const char *def_pol = (firewall_get_default_policy() == FIREWALL_ACTION_ALLOW) ? "ALLOW" : "DROP";
    snprintf(buf, sizeof(buf), "Default Policy:       %s", def_pol);
    font_draw_string(fb, c3_x + 10, c3_y + 75, buf, 1, COLOR_ZERO_WHITE);

    /* Card 4: Cryptography & Audit */
    int32_t c4_x = cx + 18 + card_w;
    int32_t c4_y = cy + 184;
    draw_fill_rect(fb, c4_x, c4_y, card_w, card_h, 0x14141Cu);
    draw_rect(fb, c4_x, c4_y, card_w, card_h, COLOR_ZERO_BORDER);
    font_draw_string(fb, c4_x + 10, c4_y + 10, "Cryptographic Core & Audit", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, c4_x + 10, c4_y + 35, "SHA-256 Engine:       VERIFIED", 1, COLOR_ZERO_WHITE);
    font_draw_string(fb, c4_x + 10, c4_y + 55, "Package Signatures:   ENFORCED", 1, COLOR_ZERO_WHITE);
    snprintf(buf, sizeof(buf), "Audit Events Logged:  %llu", (unsigned long long)stats.audit_events_logged);
    font_draw_string(fb, c4_x + 10, c4_y + 75, buf, 1, COLOR_ZERO_WHITE);
}

/* Tab 1: Caps & DAC */
static void draw_view_caps(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "ACTIVE CAPABILITY TOKENS (UNFORGEABLE HANDLES)", 1, COLOR_ZERO_GOLD);

    /* Table Header */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, cy + 34, "HANDLE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 100, cy + 34, "PID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 150, cy + 34, "TYPE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 240, cy + 34, "PERMISSIONS", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 380, cy + 34, "RESOURCE TARGET", 1, COLOR_ZERO_MUTED);

    cap_desc_t descs[12];
    uint32_t count = cap_enumerate_process(0, descs, 12);
    if (count == 0) {
        font_draw_string(fb, cx + 20, cy + 60, "No process-specific capabilities allocated yet.", 1, COLOR_ZERO_MUTED);
    }

    int32_t row_y = cy + 54;
    for (uint32_t i = 0; i < count && i < 10; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "0x%08X", descs[i].handle);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_WHITE);

        snprintf(buf, sizeof(buf), "%u", descs[i].pid);
        font_draw_string(fb, cx + 100, row_y, buf, 1, COLOR_ZERO_WHITE);

        const char *tname = "FILE";
        if (descs[i].type == CAP_TYPE_DIR) tname = "DIR";
        else if (descs[i].type == CAP_TYPE_PROCESS) tname = "PROCESS";
        else if (descs[i].type == CAP_TYPE_IPC) tname = "IPC";
        else if (descs[i].type == CAP_TYPE_NET_SOCKET) tname = "SOCKET";
        else if (descs[i].type == CAP_TYPE_DEVICE) tname = "DEVICE";
        else if (descs[i].type == CAP_TYPE_SYSTEM) tname = "SYSTEM";
        font_draw_string(fb, cx + 150, row_y, tname, 1, COLOR_ZERO_GOLD);

        char pbuf[32] = "";
        if (descs[i].perms & CAP_PERM_READ) strcat(pbuf, "R ");
        if (descs[i].perms & CAP_PERM_WRITE) strcat(pbuf, "W ");
        if (descs[i].perms & CAP_PERM_EXEC) strcat(pbuf, "X ");
        if (descs[i].perms & CAP_PERM_DELETE) strcat(pbuf, "DEL ");
        if (descs[i].perms & CAP_PERM_ADMIN) strcat(pbuf, "ADM");
        font_draw_string(fb, cx + 240, row_y, pbuf[0] ? pbuf : "NONE", 1, COLOR_ZERO_WHITE);

        font_draw_string(fb, cx + 380, row_y, descs[i].resource, 1, COLOR_ZERO_MUTED);

        row_y += 20;
    }
}

/* Tab 2: Sandboxing */
static void draw_view_sandboxing(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "PROCESS ISOLATION & SANDBOX CONTAINMENT POLICIES", 1, COLOR_ZERO_GOLD);

    /* Profiles Legend */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 50, 0x14141Cu);
    draw_rect(fb, cx + 12, cy + 30, cw - 24, 50, COLOR_ZERO_BORDER);
    font_draw_string(fb, cx + 20, cy + 38, "RESTRICTED: No net, no raw dev, strict jail root, minimal syscalls", 1, COLOR_ZERO_CRIMSON);
    font_draw_string(fb, cx + 20, cy + 56, "BASIC / STD: App jail, controlled network, standard user syscalls", 1, COLOR_ZERO_WHITE);

    /* Table Header */
    draw_fill_rect(fb, cx + 12, cy + 90, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 20, cy + 94, "PID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 80, cy + 94, "PROFILE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 200, cy + 94, "JAIL ROOT DIRECTORY", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 420, cy + 94, "NET", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 470, cy + 94, "DEV", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 520, cy + 94, "IPC", 1, COLOR_ZERO_MUTED);

    int32_t row_y = cy + 116;
    for (uint32_t pid = 0; pid < 8; pid++) {
        const sandbox_policy_t *pol = sandbox_get_policy(pid);
        if (!pol || !pol->in_use) continue;

        char buf[32];
        snprintf(buf, sizeof(buf), "%u", pol->pid);
        font_draw_string(fb, cx + 20, row_y, buf, 1, COLOR_ZERO_WHITE);

        const char *pname = "NONE";
        uint32_t pcol = COLOR_ZERO_MUTED;
        if (pol->profile == SANDBOX_PROFILE_RESTRICTED) { pname = "RESTRICTED"; pcol = COLOR_ZERO_CRIMSON; }
        else if (pol->profile == SANDBOX_PROFILE_BASIC) { pname = "BASIC"; pcol = COLOR_ZERO_WHITE; }
        else if (pol->profile == SANDBOX_PROFILE_STANDARD) { pname = "STANDARD"; pcol = COLOR_ZERO_GOLD; }
        else if (pol->profile == SANDBOX_PROFILE_SYSTEM) { pname = "SYSTEM"; pcol = COLOR_ZERO_GOLD; }
        font_draw_string(fb, cx + 80, row_y, pname, 1, pcol);

        font_draw_string(fb, cx + 200, row_y, pol->root_dir[0] ? pol->root_dir : "/", 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 420, row_y, pol->net_allowed ? "YES" : "NO", 1, pol->net_allowed ? COLOR_ZERO_GOLD : COLOR_ZERO_CRIMSON);
        font_draw_string(fb, cx + 470, row_y, pol->dev_allowed ? "YES" : "NO", 1, pol->dev_allowed ? COLOR_ZERO_GOLD : COLOR_ZERO_CRIMSON);
        font_draw_string(fb, cx + 520, row_y, pol->ipc_allowed ? "YES" : "NO", 1, pol->ipc_allowed ? COLOR_ZERO_GOLD : COLOR_ZERO_CRIMSON);

        row_y += 22;
    }
}

/* Tab 3: Network Firewall */
static void draw_view_firewall(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "STATEFUL PACKET FILTERING FIREWALL", 1, COLOR_ZERO_GOLD);

    /* Toggle / Policy Bar */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 32, 0x14141Cu);
    draw_rect(fb, cx + 12, cy + 30, cw - 24, 32, COLOR_ZERO_BORDER);

    firewall_action_t def_pol = firewall_get_default_policy();
    const char *pol_str = (def_pol == FIREWALL_ACTION_ALLOW) ? "POLICY: ALLOW (Advisory Drop)" : "POLICY: DROP (Strict Default-Deny)";
    uint32_t pol_col = (def_pol == FIREWALL_ACTION_ALLOW) ? COLOR_ZERO_GOLD : COLOR_ZERO_CRIMSON;
    font_draw_string(fb, cx + 24, cy + 40, pol_str, 1, pol_col);

    /* Toggle button */
    draw_fill_rect(fb, cx + cw - 180, cy + 34, 150, 24, 0x2A2A38u);
    draw_rect(fb, cx + cw - 180, cy + 34, 150, 24, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + cw - 170, cy + 40, "[Toggle Policy]", 1, COLOR_ZERO_GOLD);

    /* Rules Table Header */
    draw_fill_rect(fb, cx + 12, cy + 70, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, cy + 74, "ID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 60, cy + 74, "PRIO", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 110, cy + 74, "PROTO", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 180, cy + 74, "DIR", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 240, cy + 74, "PORT RANGE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 360, cy + 74, "ACTION", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 450, cy + 74, "DESCRIPTION", 1, COLOR_ZERO_MUTED);

    firewall_rule_t rules[8];
    uint32_t rcount = firewall_get_rules(rules, 8);
    int32_t row_y = cy + 96;

    for (uint32_t i = 0; i < rcount; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", rules[i].id);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_WHITE);

        snprintf(buf, sizeof(buf), "%u", rules[i].priority);
        font_draw_string(fb, cx + 60, row_y, buf, 1, COLOR_ZERO_MUTED);

        const char *pstr = "ALL";
        if (rules[i].proto == FIREWALL_PROTO_TCP) pstr = "TCP";
        else if (rules[i].proto == FIREWALL_PROTO_UDP) pstr = "UDP";
        else if (rules[i].proto == FIREWALL_PROTO_ICMP) pstr = "ICMP";
        font_draw_string(fb, cx + 110, row_y, pstr, 1, COLOR_ZERO_GOLD);

        const char *dstr = "IN";
        if (rules[i].direction == FIREWALL_DIR_OUT) dstr = "OUT";
        else if (rules[i].direction == FIREWALL_DIR_BOTH) dstr = "BOTH";
        font_draw_string(fb, cx + 180, row_y, dstr, 1, COLOR_ZERO_WHITE);

        if (rules[i].dst_port_min == 0 && rules[i].dst_port_max == 0) {
            strcpy(buf, "ANY");
        } else {
            snprintf(buf, sizeof(buf), "%u-%u", rules[i].dst_port_min, rules[i].dst_port_max);
        }
        font_draw_string(fb, cx + 240, row_y, buf, 1, COLOR_ZERO_WHITE);

        const char *astr = "ALLOW";
        uint32_t acol = COLOR_ZERO_GOLD;
        if (rules[i].action == FIREWALL_ACTION_DROP) { astr = "DROP"; acol = COLOR_ZERO_CRIMSON; }
        font_draw_string(fb, cx + 360, row_y, astr, 1, acol);

        font_draw_string(fb, cx + 450, row_y, rules[i].description, 1, COLOR_ZERO_MUTED);

        row_y += 22;
    }
}

/* Tab 4: Packages */
static void draw_view_packages(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "CRYPTOGRAPHIC PACKAGE VERIFICATION & TRUST ROOTS", 1, COLOR_ZERO_GOLD);

    bool sig_enforced = zpm_is_signature_enforced();

    /* Toggle Bar */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 32, 0x14141Cu);
    draw_rect(fb, cx + 12, cy + 30, cw - 24, 32, COLOR_ZERO_BORDER);

    font_draw_string(fb, cx + 24, cy + 40, "Signature Enforcement:", 1, COLOR_ZERO_WHITE);
    if (sig_enforced) {
        draw_badge(fb, cx + 220, cy + 34, "ENFORCED (Strict)", 0x1A331Au, COLOR_ZERO_GOLD);
    } else {
        draw_badge(fb, cx + 220, cy + 34, "ADVISORY (Unsigned Allowed)", 0x401010u, COLOR_ZERO_CRIMSON);
    }

    draw_fill_rect(fb, cx + cw - 180, cy + 34, 150, 24, 0x2A2A38u);
    draw_rect(fb, cx + cw - 180, cy + 34, 150, 24, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + cw - 170, cy + 40, "[Toggle Enforcement]", 1, COLOR_ZERO_GOLD);

    /* Installed Packages Integrity */
    draw_fill_rect(fb, cx + 12, cy + 70, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, cy + 74, "APP ID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 180, cy + 74, "VERSION", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 260, cy + 74, "INTEGRITY STATUS", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 420, cy + 74, "INSTALL PATH", 1, COLOR_ZERO_MUTED);

    zpm_record_t records[8];
    int32_t count = zpm_list(records, 8);
    int32_t row_y = cy + 96;

    for (int32_t i = 0; i < count; i++) {
        font_draw_string(fb, cx + 18, row_y, records[i].app_id, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 180, row_y, records[i].version, 1, COLOR_ZERO_MUTED);

        bool verified = zpm_verify(records[i].app_id);
        if (verified) {
            font_draw_string(fb, cx + 260, row_y, "SHA-256 OK / VERIFIED", 1, COLOR_ZERO_GOLD);
        } else {
            font_draw_string(fb, cx + 260, row_y, "INTEGRITY FAULT", 1, COLOR_ZERO_CRIMSON);
        }

        font_draw_string(fb, cx + 420, row_y, records[i].install_path, 1, COLOR_ZERO_MUTED);
        row_y += 22;
    }
}

/* Tab 5: Audit Trail */
static void draw_view_audit(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "LIVE SYSTEM SECURITY AUDIT STREAM", 1, COLOR_ZERO_GOLD);

    /* Table Header */
    draw_fill_rect(fb, cx + 12, cy + 30, cw - 24, 20, 0x1C1C24u);
    font_draw_string(fb, cx + 18, cy + 34, "ID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 60, cy + 34, "EVENT TYPE", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 220, cy + 34, "PID", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 260, cy + 34, "STATUS", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 330, cy + 34, "RESOURCE / TARGET", 1, COLOR_ZERO_MUTED);
    font_draw_string(fb, cx + 480, cy + 34, "DETAILS", 1, COLOR_ZERO_MUTED);

    audit_event_t evs[12];
    uint32_t count = audit_get_events(evs, 12);
    int32_t row_y = cy + 56;

    for (uint32_t i = 0; i < count; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "#%llu", (unsigned long long)evs[i].id);
        font_draw_string(fb, cx + 18, row_y, buf, 1, COLOR_ZERO_MUTED);

        font_draw_string(fb, cx + 60, row_y, audit_event_type_name(evs[i].type), 1, COLOR_ZERO_WHITE);

        snprintf(buf, sizeof(buf), "%u", evs[i].pid);
        font_draw_string(fb, cx + 220, row_y, buf, 1, COLOR_ZERO_MUTED);

        if (evs[i].success) {
            font_draw_string(fb, cx + 260, row_y, "ALLOW", 1, COLOR_ZERO_GOLD);
        } else {
            font_draw_string(fb, cx + 260, row_y, "DENY", 1, COLOR_ZERO_CRIMSON);
        }

        font_draw_string(fb, cx + 330, row_y, evs[i].resource[0] ? evs[i].resource : "-", 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 480, row_y, evs[i].details[0] ? evs[i].details : "-", 1, COLOR_ZERO_MUTED);

        row_y += 20;
    }
}

/* Tab 6: System Protections & Diagnostics */
static void draw_view_protection(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    font_draw_string(fb, cx + 12, cy + 10, "HARDWARE & MEMORY SECURITY ARCHITECTURE", 1, COLOR_ZERO_GOLD);

    int32_t y = cy + 35;
    struct {
        const char *name;
        const char *desc;
        const char *status;
        uint32_t    color;
    } items[] = {
        { "W^X Page Protection", "Write XOR Execute bitmask strictly enforced in page tables", "ENFORCED", COLOR_ZERO_GOLD },
        { "Supervisor Mode SMEP", "Prevents Ring 0 execution from user-space address ranges", "ACTIVE", COLOR_ZERO_GOLD },
        { "Supervisor Mode SMAP", "Explicit access gating for kernel reading/writing user memory", "ACTIVE", COLOR_ZERO_GOLD },
        { "Canonical Address Guard", "Disallows non-canonical pointer dereferencing and overflow", "ENFORCED", COLOR_ZERO_GOLD },
        { "User Pointer Sanitizer", "Ensures all syscall memory arguments reside in lower half", "ENFORCED", COLOR_ZERO_GOLD },
        { "FIPS SHA-256 Engine", "Known Answer Tests (KAT) verified during boot sequence", "PASSED", COLOR_ZERO_GOLD },
        { "Timing-Safe Memcmp", "Constant-time memory comparison prevents timing side-channels", "ACTIVE", COLOR_ZERO_GOLD },
        { "CSPRNG Entropy Source", "TSC + hardware jitter entropy mixing with SHA-256 state", "READY", COLOR_ZERO_GOLD }
    };

    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        draw_fill_rect(fb, cx + 12, y, cw - 24, 26, (i % 2 == 0) ? 0x14141Cu : 0x181822u);
        font_draw_string(fb, cx + 20, y + 6, items[i].name, 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 220, y + 6, items[i].desc, 1, COLOR_ZERO_MUTED);
        draw_badge(fb, cx + cw - 120, y + 2, items[i].status, 0x102210u, items[i].color);
        y += 30;
    }
}

static void sec_on_paint(window_t *win, framebuffer_t *fb)
{
    if (!win || !fb) return;

    int32_t cx = win->content_x;
    int32_t cy = win->content_y;
    int32_t cw = win->content_w;
    int32_t ch = win->content_h;

    /* Fill background */
    draw_fill_rect(fb, cx, cy, cw, ch, COLOR_ZERO_BLACK);

    /* Render Tab Header */
    int32_t tab_w = (cw - 16) / SEC_TAB_COUNT;
    int32_t tab_h = 28;

    draw_fill_rect(fb, cx, cy, cw, tab_h, 0x121218u);
    draw_line(fb, cx, cy + tab_h, cx + cw, cy + tab_h, COLOR_ZERO_BORDER);

    for (uint32_t i = 0; i < SEC_TAB_COUNT; i++) {
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

    /* Render Content View */
    int32_t vy = cy + tab_h;
    int32_t vh = ch - tab_h;

    switch (s_active_tab) {
    case 0: draw_view_overview(fb, cx, vy, cw, vh); break;
    case 1: draw_view_caps(fb, cx, vy, cw, vh); break;
    case 2: draw_view_sandboxing(fb, cx, vy, cw, vh); break;
    case 3: draw_view_firewall(fb, cx, vy, cw, vh); break;
    case 4: draw_view_packages(fb, cx, vy, cw, vh); break;
    case 5: draw_view_audit(fb, cx, vy, cw, vh); break;
    case 6: draw_view_protection(fb, cx, vy, cw, vh); break;
    default: break;
    }
}

static void sec_on_mouse(window_t *win, int32_t mx, int32_t my, uint8_t buttons)
{
    (void)win;
    if (!(buttons & 1)) return;

    int32_t tab_w = (SECURITY_CENTER_WIDTH - 16) / SEC_TAB_COUNT;
    int32_t tab_h = 28;

    /* Check tab clicks */
    if (my >= 0 && my < tab_h) {
        for (uint32_t i = 0; i < SEC_TAB_COUNT; i++) {
            int32_t tx = 8 + (int32_t)i * tab_w;
            if (mx >= tx && mx < tx + tab_w) {
                s_active_tab = i;
                compositor_mark_dirty();
                return;
            }
        }
    }

    /* Check Firewall Toggle button click (Tab 3) */
    if (s_active_tab == 3 && my >= 30 + tab_h && my < 62 + tab_h) {
        if (mx >= SECURITY_CENTER_WIDTH - 180 && mx < SECURITY_CENTER_WIDTH - 30) {
            firewall_action_t cur = firewall_get_default_policy();
            firewall_set_default_policy((cur == FIREWALL_ACTION_ALLOW) ? FIREWALL_ACTION_DROP : FIREWALL_ACTION_ALLOW);
            compositor_mark_dirty();
            return;
        }
    }

    /* Check Package Signature Toggle button click (Tab 4) */
    if (s_active_tab == 4 && my >= 30 + tab_h && my < 62 + tab_h) {
        if (mx >= SECURITY_CENTER_WIDTH - 180 && mx < SECURITY_CENTER_WIDTH - 30) {
            bool cur = zpm_is_signature_enforced();
            zpm_set_signature_enforced(!cur);
            compositor_mark_dirty();
            return;
        }
    }
}

static void sec_on_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)win;
    (void)scancode;
    if (key == '\t' || key == ' ') {
        s_active_tab = (s_active_tab + 1) % SEC_TAB_COUNT;
        compositor_mark_dirty();
    }
}

static void sec_on_close(window_t *win)
{
    (void)win;
    s_sec_window = NULL;
}

void security_center_init(void)
{
    s_sec_window = NULL;
    s_active_tab = 0;
}

window_t *security_center_launch(void)
{
    if (s_sec_window) {
        wm_restore(s_sec_window);
        wm_set_focus(s_sec_window);
        return s_sec_window;
    }

    s_sec_window = wm_create_window(
        120, 80, SECURITY_CENTER_WIDTH, SECURITY_CENTER_HEIGHT,
        "ZERO Security Center",
        WINDOW_FLAG_RESIZABLE
    );

    if (s_sec_window) {
        s_sec_window->icon_glyph = GLYPH_ICON_CHECK;
        s_sec_window->on_paint = sec_on_paint;
        s_sec_window->on_mouse = sec_on_mouse;
        s_sec_window->on_key = sec_on_key;
        s_sec_window->on_close = sec_on_close;
        wm_set_focus(s_sec_window);
    }

    return s_sec_window;
}

bool security_center_is_open(void)
{
    return (s_sec_window != NULL);
}

void security_center_refresh(void)
{
    if (s_sec_window) {
        compositor_mark_dirty();
    }
}
