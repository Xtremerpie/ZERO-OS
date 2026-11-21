/* ZERO KERNEL - Address Resolution Protocol (ARP) Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements ARP request/reply generation, parsing, and caching.
 */
#include "net/arp.h"
#include "net/ethernet.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "lib/string.h"

static arp_entry_t g_arp_table[ARP_CACHE_SIZE];
static spinlock_t  g_arp_lock;

void arp_init(void)
{
    spinlock_init(&g_arp_lock);
    spin_lock(&g_arp_lock);
    memset(g_arp_table, 0, sizeof(g_arp_table));
    spin_unlock(&g_arp_lock);
    KLOG_INFO("arp", "initialized ARP cache (%u entries)", ARP_CACHE_SIZE);
}

void arp_insert(uint32_t ip, const uint8_t *mac)
{
    if (!mac || ip == 0) return;

    spin_lock(&g_arp_lock);

    /* Update existing entry */
    for (size_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_table[i].valid && g_arp_table[i].ip == ip) {
            memcpy(g_arp_table[i].mac, mac, NET_ETH_ALEN);
            g_arp_table[i].age = 0;
            spin_unlock(&g_arp_lock);
            return;
        }
    }

    /* Find empty entry */
    for (size_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!g_arp_table[i].valid) {
            g_arp_table[i].ip = ip;
            memcpy(g_arp_table[i].mac, mac, NET_ETH_ALEN);
            g_arp_table[i].valid = true;
            g_arp_table[i].age = 0;
            spin_unlock(&g_arp_lock);
            return;
        }
    }

    /* Evict oldest entry (entry 0 fallback) */
    g_arp_table[0].ip = ip;
    memcpy(g_arp_table[0].mac, mac, NET_ETH_ALEN);
    g_arp_table[0].valid = true;
    g_arp_table[0].age = 0;

    spin_unlock(&g_arp_lock);
}

bool arp_lookup(uint32_t ip, uint8_t *out_mac)
{
    if (!out_mac || ip == 0) return false;

    /* Broadcast address check */
    if (ip == 0xFFFFFFFFu) {
        memcpy(out_mac, ETH_BROADCAST_MAC, NET_ETH_ALEN);
        return true;
    }

    spin_lock(&g_arp_lock);
    for (size_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_table[i].valid && g_arp_table[i].ip == ip) {
            memcpy(out_mac, g_arp_table[i].mac, NET_ETH_ALEN);
            spin_unlock(&g_arp_lock);
            return true;
        }
    }
    spin_unlock(&g_arp_lock);
    return false;
}

int arp_send_request(net_if_t *netif, uint32_t target_ip)
{
    if (!netif || target_ip == 0) return -1;

    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM);
    if (!buf) return -2;

    arp_hdr_t *hdr = (arp_hdr_t *)net_buf_put(buf, sizeof(arp_hdr_t));
    if (!hdr) {
        net_buf_free(buf);
        return -3;
    }

    hdr->htype = htons(ARP_HRD_ETHERNET);
    hdr->ptype = htons(ARP_PRO_IPV4);
    hdr->hlen = NET_ETH_ALEN;
    hdr->plen = 4;
    hdr->opcode = htons(ARP_OP_REQUEST);
    memcpy(hdr->sender_mac, netif->mac, NET_ETH_ALEN);
    hdr->sender_ip = netif->ip;
    memset(hdr->target_mac, 0, NET_ETH_ALEN);
    hdr->target_ip = target_ip;

    return ethernet_tx(netif, buf, ETH_BROADCAST_MAC, ETH_TYPE_ARP);
}

int arp_rx(net_if_t *netif, net_buf_t *buf)
{
    if (!netif || !buf || buf->len < sizeof(arp_hdr_t)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    const arp_hdr_t *hdr = (const arp_hdr_t *)buf->data;

    if (ntohs(hdr->htype) != ARP_HRD_ETHERNET || ntohs(hdr->ptype) != ARP_PRO_IPV4) {
        net_buf_free(buf);
        return 0;
    }

    uint16_t op = ntohs(hdr->opcode);
    arp_insert(hdr->sender_ip, hdr->sender_mac);

    if (op == ARP_OP_REQUEST && (netif->ip != 0) && (hdr->target_ip == netif->ip)) {
        /* Construct ARP Reply */
        net_buf_t *reply_buf = net_buf_alloc(NET_BUF_HEADROOM);
        if (reply_buf) {
            arp_hdr_t *rhdr = (arp_hdr_t *)net_buf_put(reply_buf, sizeof(arp_hdr_t));
            if (rhdr) {
                rhdr->htype = htons(ARP_HRD_ETHERNET);
                rhdr->ptype = htons(ARP_PRO_IPV4);
                rhdr->hlen = NET_ETH_ALEN;
                rhdr->plen = 4;
                rhdr->opcode = htons(ARP_OP_REPLY);
                memcpy(rhdr->sender_mac, netif->mac, NET_ETH_ALEN);
                rhdr->sender_ip = netif->ip;
                memcpy(rhdr->target_mac, hdr->sender_mac, NET_ETH_ALEN);
                rhdr->target_ip = hdr->sender_ip;

                ethernet_tx(netif, reply_buf, hdr->sender_mac, ETH_TYPE_ARP);
            } else {
                net_buf_free(reply_buf);
            }
        }
    }

    net_buf_free(buf);
    return 0;
}

size_t arp_get_table(arp_entry_t *out_table, size_t max_entries)
{
    if (!out_table || max_entries == 0) return 0;
    spin_lock(&g_arp_lock);
    size_t count = 0;
    for (size_t i = 0; i < ARP_CACHE_SIZE && count < max_entries; i++) {
        if (g_arp_table[i].valid) {
            memcpy(&out_table[count], &g_arp_table[i], sizeof(arp_entry_t));
            count++;
        }
    }
    spin_unlock(&g_arp_lock);
    return count;
}
