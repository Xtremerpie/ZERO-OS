/* ZERO KERNEL - Ethernet II Frame Layer (Milestone 4.0.0 Storage & Networking).
 *
 * Implements standard 802.3 Ethernet framing, EtherType dispatching (ARP/IPv4),
 * and broadcast/unicast address handling.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define ETH_TYPE_IPV4   0x0800u
#define ETH_TYPE_ARP    0x0806u

typedef struct __attribute__((packed)) eth_hdr {
    uint8_t  dst_mac[NET_ETH_ALEN];
    uint8_t  src_mac[NET_ETH_ALEN];
    uint16_t ethertype; /* Network byte order */
} eth_hdr_t;

extern const uint8_t ETH_BROADCAST_MAC[NET_ETH_ALEN];

/* Process an incoming Ethernet frame */
int ethernet_rx(net_if_t *netif, net_buf_t *buf);

/* Wrap payload with Ethernet header and transmit to destination MAC */
int ethernet_tx(net_if_t *netif, net_buf_t *buf, const uint8_t *dst_mac, uint16_t ethertype);
