/* ZERO KERNEL - Ethernet II Frame Layer Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements standard Ethernet encapsulation and demultiplexing.
 */
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "zero/log.h"
#include "zero/string.h"

const uint8_t ETH_BROADCAST_MAC[NET_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int ethernet_rx(net_if_t *netif, net_buf_t *buf)
{
    if (!netif || !buf || buf->len < sizeof(eth_hdr_t)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    const eth_hdr_t *hdr = (const eth_hdr_t *)buf->data;
    uint16_t ethertype = ntohs(hdr->ethertype);

    /* Verify frame is destined for this host or broadcast */
    bool is_for_us = (memcmp(hdr->dst_mac, netif->mac, NET_ETH_ALEN) == 0);
    bool is_bcast = (memcmp(hdr->dst_mac, ETH_BROADCAST_MAC, NET_ETH_ALEN) == 0);

    if (!is_for_us && !is_bcast) {
        net_buf_free(buf);
        return 0; /* Ignore frames intended for other hosts */
    }

    /* Strip Ethernet header */
    net_buf_pull(buf, sizeof(eth_hdr_t));

    switch (ethertype) {
        case ETH_TYPE_ARP:
            return arp_rx(netif, buf);
        case ETH_TYPE_IPV4:
            return ipv4_rx(netif, buf);
        default:
            net_buf_free(buf);
            return 0;
    }
}

int ethernet_tx(net_if_t *netif, net_buf_t *buf, const uint8_t *dst_mac, uint16_t ethertype)
{
    if (!netif || !buf || !dst_mac) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    eth_hdr_t *hdr = (eth_hdr_t *)net_buf_push(buf, sizeof(eth_hdr_t));
    if (!hdr) {
        net_buf_free(buf);
        return -2;
    }

    memcpy(hdr->dst_mac, dst_mac, NET_ETH_ALEN);
    memcpy(hdr->src_mac, netif->mac, NET_ETH_ALEN);
    hdr->ethertype = htons(ethertype);

    return net_tx(netif, buf);
}
