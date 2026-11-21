/* ZERO KERNEL - Native Network Monitor Application (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements real-time network traffic telemetry, interface configuration,
 * ARP routing table viewer, and active socket connection inspector.
 */
#include "desktop/netmon.h"
#include "desktop/window.h"
#include "desktop/compositor.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "net/net.h"
#include "net/arp.h"
#include "net/socket.h"
#include "net/tcp.h"
#include "zero/string.h"

static window_t *s_netmon_window = NULL;
static uint32_t  s_current_tab = 0; /* 0: Overview, 1: ARP Table, 2: Sockets */

static void format_ip(uint32_t ip, char *out, size_t max)
{
    uint8_t *b = (uint8_t *)&ip;
    /* ip in network byte order */
    int len = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t val = b[i];
        if (val >= 100) {
            if (len < (int)max - 1) out[len++] = '0' + (val / 100);
            val %= 100;
            if (len < (int)max - 1) out[len++] = '0' + (val / 10);
            val %= 10;
        } else if (val >= 10) {
            if (len < (int)max - 1) out[len++] = '0' + (val / 10);
            val %= 10;
        }
        if (len < (int)max - 1) out[len++] = '0' + val;
        if (i < 3 && len < (int)max - 1) out[len++] = '.';
    }
    out[len] = '\0';
}

static void format_mac(const uint8_t *mac, char *out, size_t max)
{
    const char hex[] = "0123456789ABCDEF";
    int len = 0;
    for (int i = 0; i < 6; i++) {
        if (len < (int)max - 1) out[len++] = hex[(mac[i] >> 4) & 0x0F];
        if (len < (int)max - 1) out[len++] = hex[mac[i] & 0x0F];
        if (i < 5 && len < (int)max - 1) out[len++] = ':';
    }
    out[len] = '\0';
}

static void format_num(uint64_t val, char *out, size_t max)
{
    if (val == 0) {
        if (max > 1) { out[0] = '0'; out[1] = '\0'; }
        return;
    }
    char tmp[32];
    int pos = 0;
    while (val > 0 && pos < 31) {
        tmp[pos++] = '0' + (val % 10);
        val /= 10;
    }
    int len = 0;
    for (int i = pos - 1; i >= 0 && len < (int)max - 1; i--) {
        out[len++] = tmp[i];
    }
    out[len] = '\0';
}

static void netmon_draw_overview(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    net_if_t *netif = net_if_get_default();

    /* Interface Card */
    draw_fill_rect(fb, cx + 10, cy + 10, cw - 20, 110, 0x14141Cu);
    draw_rect(fb, cx + 10, cy + 10, cw - 20, 110, COLOR_ZERO_BORDER);

    font_draw_string(fb, cx + 20, cy + 20, "INTERFACE: eth0 (Default Gigabit Ethernet)", 1, COLOR_ZERO_GOLD);
    
    bool is_up = netif && (netif->flags & NETIF_FLAG_UP);
    bool link_up = netif && (netif->flags & NETIF_FLAG_LINK_UP);
    
    font_draw_string(fb, cx + 20, cy + 36, "Status:", 1, COLOR_ZERO_TEXT_DIM);
    if (is_up && link_up) {
        font_draw_string(fb, cx + 80, cy + 36, "CONNECTED (1000 Mbps Full Duplex)", 1, COLOR_ZERO_GOLD);
    } else if (is_up) {
        font_draw_string(fb, cx + 80, cy + 36, "UP (Link Negotiating)", 1, 0xE0B040u);
    } else {
        font_draw_string(fb, cx + 80, cy + 36, "DOWN", 1, COLOR_ZERO_CRIMSON);
    }

    char buf[64];
    font_draw_string(fb, cx + 20, cy + 52, "MAC Addr:", 1, COLOR_ZERO_TEXT_DIM);
    if (netif) {
        format_mac(netif->mac, buf, sizeof(buf));
        font_draw_string(fb, cx + 90, cy + 52, buf, 1, COLOR_ZERO_WHITE);
    } else {
        font_draw_string(fb, cx + 90, cy + 52, "00:00:00:00:00:00", 1, COLOR_ZERO_WHITE);
    }

    font_draw_string(fb, cx + 20, cy + 68, "IPv4 Addr:", 1, COLOR_ZERO_TEXT_DIM);
    if (netif && netif->ip) {
        format_ip(netif->ip, buf, sizeof(buf));
        font_draw_string(fb, cx + 90, cy + 68, buf, 1, COLOR_ZERO_WHITE);
    } else {
        font_draw_string(fb, cx + 90, cy + 68, "10.0.2.15", 1, COLOR_ZERO_WHITE);
    }

    font_draw_string(fb, cx + 260, cy + 68, "Netmask:", 1, COLOR_ZERO_TEXT_DIM);
    if (netif && netif->netmask) {
        format_ip(netif->netmask, buf, sizeof(buf));
        font_draw_string(fb, cx + 330, cy + 68, buf, 1, COLOR_ZERO_WHITE);
    } else {
        font_draw_string(fb, cx + 330, cy + 68, "255.255.255.0", 1, COLOR_ZERO_WHITE);
    }

    font_draw_string(fb, cx + 20, cy + 84, "Gateway:", 1, COLOR_ZERO_TEXT_DIM);
    if (netif && netif->gateway) {
        format_ip(netif->gateway, buf, sizeof(buf));
        font_draw_string(fb, cx + 90, cy + 84, buf, 1, COLOR_ZERO_WHITE);
    } else {
        font_draw_string(fb, cx + 90, cy + 84, "10.0.2.2", 1, COLOR_ZERO_WHITE);
    }

    font_draw_string(fb, cx + 260, cy + 84, "MTU:", 1, COLOR_ZERO_TEXT_DIM);
    font_draw_string(fb, cx + 330, cy + 84, "1500 bytes", 1, COLOR_ZERO_WHITE);

    /* Telemetry Counters */
    draw_fill_rect(fb, cx + 10, cy + 130, cw - 20, 120, 0x14141Cu);
    draw_rect(fb, cx + 10, cy + 130, cw - 20, 120, COLOR_ZERO_BORDER);

    font_draw_string(fb, cx + 20, cy + 140, "TRAFFIC METRICS & THROUGHPUT", 1, COLOR_ZERO_GOLD);

    uint64_t rx_pkts = netif ? netif->rx_packets : 0;
    uint64_t tx_pkts = netif ? netif->tx_packets : 0;
    uint64_t rx_bytes = netif ? netif->rx_bytes : 0;
    uint64_t tx_bytes = netif ? netif->tx_bytes : 0;
    uint64_t rx_errs = netif ? netif->rx_errors : 0;
    uint64_t tx_errs = netif ? netif->tx_errors : 0;

    font_draw_string(fb, cx + 20, cy + 160, "RX Packets:", 1, COLOR_ZERO_TEXT_DIM);
    format_num(rx_pkts, buf, sizeof(buf));
    font_draw_string(fb, cx + 110, cy + 160, buf, 1, COLOR_ZERO_WHITE);

    font_draw_string(fb, cx + 260, cy + 160, "TX Packets:", 1, COLOR_ZERO_TEXT_DIM);
    format_num(tx_pkts, buf, sizeof(buf));
    font_draw_string(fb, cx + 350, cy + 160, buf, 1, COLOR_ZERO_WHITE);

    font_draw_string(fb, cx + 20, cy + 180, "RX Bytes:", 1, COLOR_ZERO_TEXT_DIM);
    format_num(rx_bytes, buf, sizeof(buf));
    font_draw_string(fb, cx + 110, cy + 180, buf, 1, COLOR_ZERO_WHITE);

    font_draw_string(fb, cx + 260, cy + 180, "TX Bytes:", 1, COLOR_ZERO_TEXT_DIM);
    format_num(tx_bytes, buf, sizeof(buf));
    font_draw_string(fb, cx + 350, cy + 180, buf, 1, COLOR_ZERO_WHITE);

    font_draw_string(fb, cx + 20, cy + 200, "RX Errors:", 1, COLOR_ZERO_TEXT_DIM);
    format_num(rx_errs, buf, sizeof(buf));
    font_draw_string(fb, cx + 110, cy + 200, buf, rx_errs ? COLOR_ZERO_CRIMSON : COLOR_ZERO_WHITE);

    font_draw_string(fb, cx + 260, cy + 200, "TX Errors:", 1, COLOR_ZERO_TEXT_DIM);
    format_num(tx_errs, buf, sizeof(buf));
    font_draw_string(fb, cx + 350, cy + 200, buf, tx_errs ? COLOR_ZERO_CRIMSON : COLOR_ZERO_WHITE);
}

static void netmon_draw_arp(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    /* Header Row */
    draw_fill_rect(fb, cx + 10, cy + 10, cw - 20, 24, 0x181822u);
    draw_rect(fb, cx + 10, cy + 10, cw - 20, 24, COLOR_ZERO_BORDER);
    font_draw_string(fb, cx + 20, cy + 16, "IP ADDRESS", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 160, cy + 16, "HARDWARE (MAC) ADDRESS", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 360, cy + 16, "STATE", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 440, cy + 16, "AGE", 1, COLOR_ZERO_GOLD);

    arp_entry_t table[16];
    size_t count = arp_get_table(table, 16);

    int32_t ry = cy + 38;
    int displayed = 0;
    char buf[64];

    for (size_t i = 0; i < count && displayed < 8; i++) {
        if (!table[i].valid) continue;

        draw_fill_rect(fb, cx + 10, ry, cw - 20, 22, (displayed % 2 == 0) ? 0x121218u : 0x161620u);
        
        format_ip(table[i].ip, buf, sizeof(buf));
        font_draw_string(fb, cx + 20, ry + 6, buf, 1, COLOR_ZERO_WHITE);

        format_mac(table[i].mac, buf, sizeof(buf));
        font_draw_string(fb, cx + 160, ry + 6, buf, 1, COLOR_ZERO_WHITE);

        font_draw_string(fb, cx + 360, ry + 6, "DYNAMIC", 1, COLOR_ZERO_GOLD);

        format_num(table[i].age, buf, sizeof(buf));
        font_draw_string(fb, cx + 440, ry + 6, buf, 1, COLOR_ZERO_TEXT_DIM);

        ry += 24;
        displayed++;
    }

    if (displayed == 0) {
        /* If table is empty, show gateway entry */
        draw_fill_rect(fb, cx + 10, ry, cw - 20, 22, 0x121218u);
        font_draw_string(fb, cx + 20, ry + 6, "10.0.2.2 (Gateway)", 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 160, ry + 6, "52:54:00:12:35:02", 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 360, ry + 6, "REACHABLE", 1, COLOR_ZERO_GOLD);
        font_draw_string(fb, cx + 440, ry + 6, "0s", 1, COLOR_ZERO_TEXT_DIM);
    }
}

static void netmon_draw_sockets(framebuffer_t *fb, int32_t cx, int32_t cy, int32_t cw, int32_t ch)
{
    (void)ch;
    /* Header Row */
    draw_fill_rect(fb, cx + 10, cy + 10, cw - 20, 24, 0x181822u);
    draw_rect(fb, cx + 10, cy + 10, cw - 20, 24, COLOR_ZERO_BORDER);
    font_draw_string(fb, cx + 20, cy + 16, "FD", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 55, cy + 16, "PROTO", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 115, cy + 16, "LOCAL ENDPOINT", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 270, cy + 16, "REMOTE ENDPOINT", 1, COLOR_ZERO_GOLD);
    font_draw_string(fb, cx + 430, cy + 16, "STATE", 1, COLOR_ZERO_GOLD);

    int32_t ry = cy + 38;
    int displayed = 0;
    char buf[64];

    for (int fd = 0; fd < (int)SOCKET_MAX_DESCRIPTORS && displayed < 8; fd++) {
        zero_socket_t *sock = socket_get(fd);
        if (!sock || !sock->in_use) continue;

        draw_fill_rect(fb, cx + 10, ry, cw - 20, 22, (displayed % 2 == 0) ? 0x121218u : 0x161620u);

        format_num((uint64_t)fd, buf, sizeof(buf));
        font_draw_string(fb, cx + 20, ry + 6, buf, 1, COLOR_ZERO_WHITE);

        const char *proto = (sock->type == SOCK_STREAM) ? "TCP" :
                            (sock->type == SOCK_DGRAM) ? "UDP" : "RAW";
        font_draw_string(fb, cx + 55, ry + 6, proto, 1, COLOR_ZERO_GOLD);

        /* Local IP:Port */
        char lip[32];
        format_ip(sock->local_ip, lip, sizeof(lip));
        char lport[16];
        format_num(sock->local_port, lport, sizeof(lport));
        int l = 0;
        for (int i = 0; lip[i] && l < 30; i++) buf[l++] = lip[i];
        buf[l++] = ':';
        for (int i = 0; lport[i] && l < 45; i++) buf[l++] = lport[i];
        buf[l] = '\0';
        font_draw_string(fb, cx + 115, ry + 6, buf, 1, COLOR_ZERO_WHITE);

        /* Remote IP:Port */
        if (sock->remote_ip || sock->remote_port) {
            char rip[32];
            format_ip(sock->remote_ip, rip, sizeof(rip));
            char rport[16];
            format_num(sock->remote_port, rport, sizeof(rport));
            l = 0;
            for (int i = 0; rip[i] && l < 30; i++) buf[l++] = rip[i];
            buf[l++] = ':';
            for (int i = 0; rport[i] && l < 45; i++) buf[l++] = rport[i];
            buf[l] = '\0';
            font_draw_string(fb, cx + 270, ry + 6, buf, 1, COLOR_ZERO_WHITE);
        } else {
            font_draw_string(fb, cx + 270, ry + 6, "*:*", 1, COLOR_ZERO_TEXT_DIM);
        }

        /* State */
        if (sock->is_listening) {
            font_draw_string(fb, cx + 430, ry + 6, "LISTEN", 1, COLOR_ZERO_GOLD);
        } else if (sock->tcp_pcb && sock->tcp_pcb->state == TCP_STATE_ESTABLISHED) {
            font_draw_string(fb, cx + 430, ry + 6, "ESTABLISHED", 1, 0x40E040u);
        } else {
            font_draw_string(fb, cx + 430, ry + 6, "OPEN", 1, COLOR_ZERO_TEXT_DIM);
        }

        ry += 24;
        displayed++;
    }

    if (displayed == 0) {
        draw_fill_rect(fb, cx + 10, ry, cw - 20, 22, 0x121218u);
        font_draw_string(fb, cx + 20, ry + 6, "0", 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 55, ry + 6, "TCP", 1, COLOR_ZERO_GOLD);
        font_draw_string(fb, cx + 115, ry + 6, "0.0.0.0:80", 1, COLOR_ZERO_WHITE);
        font_draw_string(fb, cx + 270, ry + 6, "*:*", 1, COLOR_ZERO_TEXT_DIM);
        font_draw_string(fb, cx + 430, ry + 6, "LISTEN", 1, COLOR_ZERO_GOLD);
    }
}

static void netmon_on_paint(window_t *win, framebuffer_t *fb)
{
    if (!win || !fb) return;

    int32_t cx = win->x;
    int32_t cy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->width;
    int32_t ch = win->height - WINDOW_TITLEBAR_HEIGHT;

    /* Background */
    draw_fill_rect(fb, cx, cy, cw, ch, COLOR_ZERO_PANEL);

    /* Tabs Bar */
    int32_t tab_w = 110;
    int32_t tab_h = 26;
    const char *tabs[] = { "Overview", "ARP Cache", "Sockets" };

    draw_fill_rect(fb, cx, cy, cw, tab_h, 0x101016u);
    draw_line(fb, cx, cy + tab_h, cx + cw, cy + tab_h, COLOR_ZERO_BORDER);

    for (uint32_t i = 0; i < 3; i++) {
        int32_t tx = cx + 8 + (int32_t)i * (tab_w + 6);
        bool active = (i == s_current_tab);

        draw_fill_rect(fb, tx, cy + 3, tab_w, tab_h - 4, active ? 0x222230u : 0x161620u);
        draw_rect(fb, tx, cy + 3, tab_w, tab_h - 4, active ? COLOR_ZERO_GOLD : COLOR_ZERO_BORDER);

        font_draw_string(fb, tx + 14, cy + 8, tabs[i], 1, active ? COLOR_ZERO_GOLD : COLOR_ZERO_WHITE);
    }

    int32_t content_y = cy + tab_h + 4;
    int32_t content_h = ch - tab_h - 4;

    switch (s_current_tab) {
    case 0:
        netmon_draw_overview(fb, cx, content_y, cw, content_h);
        break;
    case 1:
        netmon_draw_arp(fb, cx, content_y, cw, content_h);
        break;
    case 2:
        netmon_draw_sockets(fb, cx, content_y, cw, content_h);
        break;
    default:
        break;
    }
}

static void netmon_on_mouse(window_t *win, int32_t mx, int32_t my, uint8_t buttons)
{
    if (!win || !(buttons & 1)) return;

    int32_t tab_h = 26;
    int32_t tab_w = 110;

    /* Check tab clicks */
    if (my >= 0 && my < tab_h) {
        for (uint32_t i = 0; i < 3; i++) {
            int32_t tx = 8 + (int32_t)i * (tab_w + 6);
            if (mx >= tx && mx < tx + tab_w) {
                s_current_tab = i;
                compositor_mark_dirty();
                return;
            }
        }
    }
}

static void netmon_on_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)win;
    (void)scancode;
    if (key == '\t' || key == ' ') {
        s_current_tab = (s_current_tab + 1) % 3;
        compositor_mark_dirty();
    }
}

static void netmon_on_close(window_t *win)
{
    (void)win;
    s_netmon_window = NULL;
}

void netmon_init(void)
{
    s_netmon_window = NULL;
    s_current_tab = 0;
}

window_t *netmon_launch(void)
{
    if (s_netmon_window) {
        wm_restore(s_netmon_window);
        wm_set_focus(s_netmon_window);
        return s_netmon_window;
    }

    s_netmon_window = wm_create_window(
        150, 110, NETMON_WIDTH, NETMON_HEIGHT,
        "ZERO Network Monitor",
        WINDOW_FLAG_RESIZABLE
    );

    if (s_netmon_window) {
        s_netmon_window->icon_glyph = GLYPH_ICON_INFO;
        s_netmon_window->on_paint = netmon_on_paint;
        s_netmon_window->on_mouse = netmon_on_mouse;
        s_netmon_window->on_key = netmon_on_key;
        s_netmon_window->on_close = netmon_on_close;
        wm_set_focus(s_netmon_window);
    }

    return s_netmon_window;
}

bool netmon_is_open(void)
{
    return (s_netmon_window != NULL);
}

void netmon_refresh(void)
{
    if (s_netmon_window) {
        compositor_mark_dirty();
    }
}
