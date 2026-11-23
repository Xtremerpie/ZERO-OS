/* ZERO KERNEL - Central Network Manager (Milestone 4.0.0 Storage & Networking).
 *
 * Implements interface registry, packet ingress/egress dispatch, and traffic counters.
 */
#include "net/net.h"
#include "net/net_buf.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/tcp.h"
#include "net/socket.h"
#include "security/firewall.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "zero/string.h"

static net_if_t   *g_interfaces = NULL;
static size_t      g_num_interfaces = 0;
static spinlock_t  g_net_lock;

void net_init(void)
{
    spinlock_init(&g_net_lock);
    net_buf_init();
    arp_init();
    tcp_init();
    socket_subsystem_init();
    KLOG_INFO("net", "central network subsystem initialized");
}

int net_if_register(net_if_t *netif)
{
    if (!netif) return -1;

    spin_lock(&g_net_lock);
    if (g_num_interfaces >= NET_MAX_INTERFACES) {
        spin_unlock(&g_net_lock);
        return -2;
    }

    netif->next = g_interfaces;
    g_interfaces = netif;
    g_num_interfaces++;
    spin_unlock(&g_net_lock);

    KLOG_INFO("net", "registered interface '%s' (MAC %02X:%02X:%02X:%02X:%02X:%02X, MTU %u)",
              netif->name,
              netif->mac[0], netif->mac[1], netif->mac[2],
              netif->mac[3], netif->mac[4], netif->mac[5],
              netif->mtu);
    return 0;
}

net_if_t *net_if_get_default(void)
{
    spin_lock(&g_net_lock);
    net_if_t *cur = g_interfaces;
    while (cur) {
        if ((cur->flags & NETIF_FLAG_UP) && (cur->flags & NETIF_FLAG_LINK_UP)) {
            spin_unlock(&g_net_lock);
            return cur;
        }
        cur = cur->next;
    }
    net_if_t *fallback = g_interfaces;
    spin_unlock(&g_net_lock);
    return fallback;
}

net_if_t *net_if_get_by_name(const char *name)
{
    if (!name) return NULL;
    spin_lock(&g_net_lock);
    net_if_t *cur = g_interfaces;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            spin_unlock(&g_net_lock);
            return cur;
        }
        cur = cur->next;
    }
    spin_unlock(&g_net_lock);
    return NULL;
}

size_t net_if_count(void)
{
    spin_lock(&g_net_lock);
    size_t count = g_num_interfaces;
    spin_unlock(&g_net_lock);
    return count;
}

net_if_t *net_if_get(size_t index)
{
    spin_lock(&g_net_lock);
    net_if_t *cur = g_interfaces;
    size_t i = 0;
    while (cur && i < index) {
        cur = cur->next;
        i++;
    }
    spin_unlock(&g_net_lock);
    return cur;
}

int net_rx(net_if_t *netif, net_buf_t *buf)
{
    if (!netif || !buf) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    /* ZERO Security: Packet Filtering Firewall Ingress Hook */
    if (!firewall_filter(buf, FIREWALL_DIR_IN)) {
        net_buf_free(buf);
        return -3;
    }

    netif->rx_packets++;
    netif->rx_bytes += buf->len;

    return ethernet_rx(netif, buf);
}

int net_tx(net_if_t *netif, net_buf_t *buf)
{
    if (!netif || !buf) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    /* ZERO Security: Packet Filtering Firewall Egress Hook */
    if (!firewall_filter(buf, FIREWALL_DIR_OUT)) {
        net_buf_free(buf);
        return -3;
    }

    if (!netif->send) {
        netif->tx_errors++;
        net_buf_free(buf);
        return -2;
    }

    size_t len = buf->len;
    int rc = netif->send(netif, buf);
    if (rc == 0) {
        netif->tx_packets++;
        netif->tx_bytes += len;
    } else {
        netif->tx_errors++;
    }

    net_buf_free(buf);
    return rc;
}

void net_poll_all(void)
{
    spin_lock(&g_net_lock);
    net_if_t *cur = g_interfaces;
    while (cur) {
        if (cur->poll) {
            cur->poll(cur);
        }
        cur = cur->next;
    }
    spin_unlock(&g_net_lock);
}
