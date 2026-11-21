/* ZERO KERNEL - Address Resolution Protocol (ARP) (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 826 ARP requests, ARP replies, and dynamic ARP cache table.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define ARP_HRD_ETHERNET  1u
#define ARP_PRO_IPV4      0x0800u
#define ARP_OP_REQUEST    1u
#define ARP_OP_REPLY      2u

#define ARP_CACHE_SIZE    32u

typedef struct __attribute__((packed)) arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t opcode;
    uint8_t  sender_mac[NET_ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[NET_ETH_ALEN];
    uint32_t target_ip;
} arp_hdr_t;

typedef struct arp_entry {
    uint32_t ip;
    uint8_t  mac[NET_ETH_ALEN];
    bool     valid;
    uint32_t age;
} arp_entry_t;

/* Initialize ARP subsystem and table */
void arp_init(void);

/* Process an incoming ARP frame */
int arp_rx(net_if_t *netif, net_buf_t *buf);

/* Send an ARP Request broadcasting for target_ip */
int arp_send_request(net_if_t *netif, uint32_t target_ip);

/* Query ARP cache for target IP. Returns true if found and copies MAC to out_mac. */
bool arp_lookup(uint32_t ip, uint8_t *out_mac);

/* Manually or dynamically insert/update ARP entry */
void arp_insert(uint32_t ip, const uint8_t *mac);

/* Retrieve ARP cache table for diagnostics */
size_t arp_get_table(arp_entry_t *out_table, size_t max_entries);
