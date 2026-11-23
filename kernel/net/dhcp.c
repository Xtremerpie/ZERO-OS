/* ZERO KERNEL - Dynamic Host Configuration Protocol (DHCP) Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 2131 client state transitions, option extraction, and automated network setup.
 */
#include "net/dhcp.h"
#include "net/udp.h"
#include "net/ipv4.h"
#include "zero/log.h"
#include "lib/string.h"

static dhcp_state_t g_dhcp_state = DHCP_STATE_INIT;
static uint32_t     g_dhcp_xid = 0x5A45524Fu; /* "ZERO" */

dhcp_state_t dhcp_get_state(void)
{
    return g_dhcp_state;
}

int dhcp_start(net_if_t *netif)
{
    if (!netif) return -1;

    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM + sizeof(udp_hdr_t) + sizeof(ipv4_hdr_t));
    if (!buf) return -2;

    dhcp_msg_t *msg = (dhcp_msg_t *)net_buf_put(buf, sizeof(dhcp_msg_t));
    if (!msg) {
        net_buf_free(buf);
        return -3;
    }

    memset(msg, 0, sizeof(dhcp_msg_t));
    msg->op = DHCP_BOOTREQUEST;
    msg->htype = 1;
    msg->hlen = NET_ETH_ALEN;
    msg->xid = htonl(g_dhcp_xid);
    msg->flags = htons(0x8000u); /* Broadcast flag */
    memcpy(msg->chaddr, netif->mac, NET_ETH_ALEN);
    msg->cookie = htonl(DHCP_MAGIC_COOKIE);

    /* DHCP Options */
    size_t opt_idx = 0;
    msg->options[opt_idx++] = DHCP_OPT_MSG_TYPE;
    msg->options[opt_idx++] = 1;
    msg->options[opt_idx++] = DHCP_MSG_DISCOVER;

    msg->options[opt_idx++] = 55; /* Parameter Request List */
    msg->options[opt_idx++] = 3;
    msg->options[opt_idx++] = DHCP_OPT_SUBNET_MASK;
    msg->options[opt_idx++] = DHCP_OPT_ROUTER;
    msg->options[opt_idx++] = DHCP_OPT_DNS_SERVER;

    msg->options[opt_idx++] = DHCP_OPT_END;

    g_dhcp_state = DHCP_STATE_DISCOVERING;
    KLOG_INFO("dhcp", "broadcasting DHCP Discover (xid 0x%08X)...", g_dhcp_xid);

    return udp_tx(netif, buf, DHCP_CLIENT_PORT, 0xFFFFFFFFu, DHCP_SERVER_PORT);
}

static int dhcp_send_request(net_if_t *netif, uint32_t requested_ip, uint32_t server_ip)
{
    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM + sizeof(udp_hdr_t) + sizeof(ipv4_hdr_t));
    if (!buf) return -1;

    dhcp_msg_t *msg = (dhcp_msg_t *)net_buf_put(buf, sizeof(dhcp_msg_t));
    if (!msg) {
        net_buf_free(buf);
        return -2;
    }

    memset(msg, 0, sizeof(dhcp_msg_t));
    msg->op = DHCP_BOOTREQUEST;
    msg->htype = 1;
    msg->hlen = NET_ETH_ALEN;
    msg->xid = htonl(g_dhcp_xid);
    msg->flags = htons(0x8000u);
    memcpy(msg->chaddr, netif->mac, NET_ETH_ALEN);
    msg->cookie = htonl(DHCP_MAGIC_COOKIE);

    size_t opt_idx = 0;
    msg->options[opt_idx++] = DHCP_OPT_MSG_TYPE;
    msg->options[opt_idx++] = 1;
    msg->options[opt_idx++] = DHCP_MSG_REQUEST;

    msg->options[opt_idx++] = DHCP_OPT_REQUESTED_IP;
    msg->options[opt_idx++] = 4;
    memcpy(&msg->options[opt_idx], &requested_ip, 4);
    opt_idx += 4;

    if (server_ip != 0) {
        msg->options[opt_idx++] = DHCP_OPT_SERVER_ID;
        msg->options[opt_idx++] = 4;
        memcpy(&msg->options[opt_idx], &server_ip, 4);
        opt_idx += 4;
    }

    msg->options[opt_idx++] = DHCP_OPT_END;

    g_dhcp_state = DHCP_STATE_REQUESTING;
    KLOG_INFO("dhcp", "sending DHCP Request for offered IP...");

    return udp_tx(netif, buf, DHCP_CLIENT_PORT, 0xFFFFFFFFu, DHCP_SERVER_PORT);
}

int dhcp_rx(net_if_t *netif, net_buf_t *buf)
{
    if (!netif || !buf || buf->len < (sizeof(dhcp_msg_t) - 308)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    const dhcp_msg_t *msg = (const dhcp_msg_t *)buf->data;

    if (msg->op != DHCP_BOOTREPLY || ntohl(msg->cookie) != DHCP_MAGIC_COOKIE) {
        net_buf_free(buf);
        return 0;
    }

    if (ntohl(msg->xid) != g_dhcp_xid) {
        net_buf_free(buf);
        return 0;
    }

    uint8_t msg_type = 0;
    uint32_t mask = 0;
    uint32_t router = 0;
    uint32_t dns = 0;
    uint32_t server_id = 0;

    size_t opt_offset = 0;
    while (opt_offset < sizeof(msg->options)) {
        uint8_t opt = msg->options[opt_offset++];
        if (opt == DHCP_OPT_PAD) continue;
        if (opt == DHCP_OPT_END) break;

        if (opt_offset >= sizeof(msg->options)) break;
        uint8_t len = msg->options[opt_offset++];
        if (opt_offset + len > sizeof(msg->options)) break;

        const uint8_t *val = &msg->options[opt_offset];
        opt_offset += len;

        switch (opt) {
            case DHCP_OPT_MSG_TYPE:
                if (len >= 1) msg_type = val[0];
                break;
            case DHCP_OPT_SUBNET_MASK:
                if (len >= 4) memcpy(&mask, val, 4);
                break;
            case DHCP_OPT_ROUTER:
                if (len >= 4) memcpy(&router, val, 4);
                break;
            case DHCP_OPT_DNS_SERVER:
                if (len >= 4) memcpy(&dns, val, 4);
                break;
            case DHCP_OPT_SERVER_ID:
                if (len >= 4) memcpy(&server_id, val, 4);
                break;
            default:
                break;
        }
    }

    if (msg_type == DHCP_MSG_OFFER) {
        KLOG_INFO("dhcp", "received DHCP Offer (IP: %u.%u.%u.%u)",
                  (msg->yiaddr) & 0xFF, (msg->yiaddr >> 8) & 0xFF,
                  (msg->yiaddr >> 16) & 0xFF, (msg->yiaddr >> 24) & 0xFF);
        uint32_t offered_ip = msg->yiaddr;
        net_buf_free(buf);
        return dhcp_send_request(netif, offered_ip, server_id);
    } else if (msg_type == DHCP_MSG_ACK) {
        netif->ip = msg->yiaddr;
        if (mask != 0) netif->netmask = mask;
        if (router != 0) netif->gateway = router;
        if (dns != 0) netif->dns = dns;
        netif->flags |= NETIF_FLAG_DHCP | NETIF_FLAG_UP;
        g_dhcp_state = DHCP_STATE_BOUND;

        KLOG_INFO("dhcp", "DHCP bound! IP: %u.%u.%u.%u, Mask: %u.%u.%u.%u, GW: %u.%u.%u.%u, DNS: %u.%u.%u.%u",
                  (netif->ip) & 0xFF, (netif->ip >> 8) & 0xFF,
                  (netif->ip >> 16) & 0xFF, (netif->ip >> 24) & 0xFF,
                  (netif->netmask) & 0xFF, (netif->netmask >> 8) & 0xFF,
                  (netif->netmask >> 16) & 0xFF, (netif->netmask >> 24) & 0xFF,
                  (netif->gateway) & 0xFF, (netif->gateway >> 8) & 0xFF,
                  (netif->gateway >> 16) & 0xFF, (netif->gateway >> 24) & 0xFF,
                  (netif->dns) & 0xFF, (netif->dns >> 8) & 0xFF,
                  (netif->dns >> 16) & 0xFF, (netif->dns >> 24) & 0xFF);
    }

    net_buf_free(buf);
    return 0;
}
