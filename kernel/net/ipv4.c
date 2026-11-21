/* ZERO KERNEL - Internet Protocol Version 4 (IPv4) Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 791 packet parsing, checksumming, routing, and layer-4 demultiplexing.
 */
#include "net/ipv4.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "zero/log.h"
#include "lib/string.h"

static uint16_t g_ipv4_packet_id = 1;

uint16_t ipv4_checksum(const void *data, size_t length)
{
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (length > 1) {
        sum += *p++;
        length -= 2;
    }
    if (length == 1) {
        sum += *(const uint8_t *)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

int ipv4_rx(net_if_t *netif, net_buf_t *buf)
{
    if (!netif || !buf || buf->len < sizeof(ipv4_hdr_t)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    const ipv4_hdr_t *hdr = (const ipv4_hdr_t *)buf->data;

    uint8_t version = (hdr->version_ihl >> 4) & 0x0F;
    uint8_t ihl = hdr->version_ihl & 0x0F;
    size_t hlen = ihl * 4;

    if (version != IPV4_VERSION || hlen < IPV4_MIN_HLEN || buf->len < hlen) {
        net_buf_free(buf);
        return -2;
    }

    /* Verify checksum */
    if (ipv4_checksum(hdr, hlen) != 0) {
        KLOG_WARN("ipv4", "checksum verification failed!");
        net_buf_free(buf);
        return -3;
    }

    uint16_t total_len = ntohs(hdr->total_length);
    if (buf->len > total_len) {
        buf->len = total_len;
        buf->tail = buf->data + total_len;
    }

    uint8_t proto = hdr->protocol;
    uint32_t src_ip = hdr->src_ip;
    uint32_t dst_ip = hdr->dst_ip;

    /* Strip IPv4 header */
    net_buf_pull(buf, hlen);

    switch (proto) {
        case IPV4_PROTO_ICMP:
            return icmp_rx(netif, buf, src_ip);
        case IPV4_PROTO_UDP:
            return udp_rx(netif, buf, src_ip, dst_ip);
        case IPV4_PROTO_TCP:
            return tcp_rx(netif, buf, src_ip, dst_ip);
        default:
            net_buf_free(buf);
            return 0;
    }
}

int ipv4_tx(net_if_t *netif, net_buf_t *buf, uint32_t dst_ip, uint8_t protocol)
{
    if (!netif || !buf) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    uint8_t dst_mac[NET_ETH_ALEN];
    bool have_mac = false;

    if (dst_ip == 0xFFFFFFFFu) {
        memcpy(dst_mac, ETH_BROADCAST_MAC, NET_ETH_ALEN);
        have_mac = true;
    } else if (netif->netmask != 0 && ((dst_ip & netif->netmask) == (netif->ip & netif->netmask))) {
        /* Local subnet destination */
        have_mac = arp_lookup(dst_ip, dst_mac);
        if (!have_mac) {
            arp_send_request(netif, dst_ip);
        }
    } else if (netif->gateway != 0) {
        /* Remote destination routed via default gateway */
        have_mac = arp_lookup(netif->gateway, dst_mac);
        if (!have_mac) {
            arp_send_request(netif, netif->gateway);
        }
    }

    if (!have_mac) {
        /* Fall back to broadcast if unresolved */
        memcpy(dst_mac, ETH_BROADCAST_MAC, NET_ETH_ALEN);
    }

    ipv4_hdr_t *hdr = (ipv4_hdr_t *)net_buf_push(buf, sizeof(ipv4_hdr_t));
    if (!hdr) {
        net_buf_free(buf);
        return -2;
    }

    hdr->version_ihl = 0x45; /* Version 4, 20 bytes (5 words) */
    hdr->dscp_ecn = 0;
    hdr->total_length = htons((uint16_t)buf->len);
    hdr->id = htons(g_ipv4_packet_id++);
    hdr->flags_frag = htons(IPV4_FLAG_DF);
    hdr->ttl = IPV4_DEFAULT_TTL;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = netif->ip;
    hdr->dst_ip = dst_ip;
    hdr->checksum = ipv4_checksum(hdr, sizeof(ipv4_hdr_t));

    return ethernet_tx(netif, buf, dst_mac, ETH_TYPE_IPV4);
}
