/* ZERO KERNEL - Network Diagnostic Reporting Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements formatted reporting for interfaces, traffic, ARP, sockets, and protocols.
 */
#include "net/net_report.h"
#include "net/net.h"
#include "net/arp.h"
#include "net/socket.h"
#include "net/dhcp.h"
#include "zero/log.h"
#include "zero/log.h"

void net_report_generate(void)
{
    kprintf("\n================================================================================\n");
    kprintf("                    ZERO OS NETWORK DIAGNOSTIC REPORT\n");
    kprintf("================================================================================\n");

    /* 1. Network Interfaces */
    size_t num_if = net_if_count();
    kprintf("[INTERFACES] Total registered interfaces: %u\n", (uint32_t)num_if);
    for (size_t i = 0; i < num_if; i++) {
        net_if_t *netif = net_if_get(i);
        if (!netif) continue;

        kprintf("  Interface %u: %s\n", (uint32_t)i, netif->name);
        kprintf("    MAC Address : %02X:%02X:%02X:%02X:%02X:%02X\n",
                netif->mac[0], netif->mac[1], netif->mac[2],
                netif->mac[3], netif->mac[4], netif->mac[5]);
        kprintf("    IPv4 Address: %u.%u.%u.%u\n",
                netif->ip & 0xFF, (netif->ip >> 8) & 0xFF,
                (netif->ip >> 16) & 0xFF, (netif->ip >> 24) & 0xFF);
        kprintf("    Subnet Mask : %u.%u.%u.%u\n",
                netif->netmask & 0xFF, (netif->netmask >> 8) & 0xFF,
                (netif->netmask >> 16) & 0xFF, (netif->netmask >> 24) & 0xFF);
        kprintf("    Gateway     : %u.%u.%u.%u\n",
                netif->gateway & 0xFF, (netif->gateway >> 8) & 0xFF,
                (netif->gateway >> 16) & 0xFF, (netif->gateway >> 24) & 0xFF);
        kprintf("    DNS Server  : %u.%u.%u.%u\n",
                netif->dns & 0xFF, (netif->dns >> 8) & 0xFF,
                (netif->dns >> 16) & 0xFF, (netif->dns >> 24) & 0xFF);
        kprintf("    Status      : [%s] [%s] MTU: %u\n",
                (netif->flags & NETIF_FLAG_UP) ? "UP" : "DOWN",
                (netif->flags & NETIF_FLAG_LINK_UP) ? "LINK UP" : "NO CARRIER",
                netif->mtu);
        kprintf("    Traffic     : TX: %llu pkts (%llu bytes, %llu err) | RX: %llu pkts (%llu bytes, %llu err)\n",
                (unsigned long long)netif->tx_packets, (unsigned long long)netif->tx_bytes, (unsigned long long)netif->tx_errors,
                (unsigned long long)netif->rx_packets, (unsigned long long)netif->rx_bytes, (unsigned long long)netif->rx_errors);
    }

    /* 2. ARP Cache */
    arp_entry_t arp_entries[16];
    size_t arp_count = arp_get_table(arp_entries, 16);
    kprintf("\n[ARP CACHE] Active entries: %u\n", (uint32_t)arp_count);
    for (size_t i = 0; i < arp_count; i++) {
        uint32_t ip = arp_entries[i].ip;
        kprintf("  IP: %u.%u.%u.%u -> MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF,
                arp_entries[i].mac[0], arp_entries[i].mac[1], arp_entries[i].mac[2],
                arp_entries[i].mac[3], arp_entries[i].mac[4], arp_entries[i].mac[5]);
    }

    /* 3. Sockets Table */
    kprintf("\n[SOCKETS] Kernel socket descriptors:\n");
    size_t active_socks = 0;
    for (size_t i = 0; i < SOCKET_MAX_DESCRIPTORS; i++) {
        zero_socket_t *s = socket_get((int)i);
        if (s && s->in_use) {
            active_socks++;
            const char *type_str = (s->type == SOCK_STREAM) ? "STREAM(TCP)" :
                                   (s->type == SOCK_DGRAM) ? "DGRAM(UDP)" : "RAW";
            kprintf("  FD %u: %s Local: %u.%u.%u.%u:%u | Remote: %u.%u.%u.%u:%u | %s\n",
                    (uint32_t)s->id, type_str,
                    s->local_ip & 0xFF, (s->local_ip >> 8) & 0xFF, (s->local_ip >> 16) & 0xFF, (s->local_ip >> 24) & 0xFF, s->local_port,
                    s->remote_ip & 0xFF, (s->remote_ip >> 8) & 0xFF, (s->remote_ip >> 16) & 0xFF, (s->remote_ip >> 24) & 0xFF, s->remote_port,
                    s->is_listening ? "LISTEN" : "CONNECTED/IDLE");
        }
    }
    if (active_socks == 0) {
        kprintf("  (none active)\n");
    }

    /* 4. Protocols Status */
    kprintf("\n[PROTOCOLS] Operational status:\n");
    kprintf("  - Ethernet II : ACTIVE (802.3 framing, EtherType dispatch)\n");
    kprintf("  - ARP         : ACTIVE (RFC 826 request/reply, 32-entry cache)\n");
    kprintf("  - IPv4        : ACTIVE (RFC 791, 16-bit Internet checksum, routing)\n");
    kprintf("  - ICMP        : ACTIVE (RFC 792 Echo Request/Reply responder)\n");
    kprintf("  - UDP         : ACTIVE (RFC 768 port demultiplexer)\n");
    kprintf("  - TCP         : ACTIVE (RFC 793 state machine, 3-way handshake, seq/ack)\n");
    kprintf("  - DHCP Client : ACTIVE (RFC 2131 state machine: %s)\n",
            (dhcp_get_state() == DHCP_STATE_BOUND) ? "BOUND" : "INIT/DISCOVERING");
    kprintf("  - DNS Client  : ACTIVE (RFC 1035 query builder & A-record parser)\n");

    kprintf("================================================================================\n\n");
}
