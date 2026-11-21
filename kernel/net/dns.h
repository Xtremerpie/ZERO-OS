/* ZERO KERNEL - Domain Name System (DNS) Resolver (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 1035 DNS client query formatting, QNAME domain label encoding,
 * and response parsing for IPv4 (A record) hostname resolution.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define DNS_PORT        53u
#define DNS_FLAG_QUERY  0x0100u /* Standard query, recursion desired */
#define DNS_TYPE_A      1u      /* IPv4 Host Address */
#define DNS_CLASS_IN    1u      /* Internet */

typedef struct __attribute__((packed)) dns_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_hdr_t;

/* Build a DNS query packet for the given hostname into out_buf */
int dns_build_query(const char *hostname, uint16_t tx_id, uint8_t *out_buf, size_t max_len);

/* Parse a DNS response buffer and extract the first resolved IPv4 A-record address */
int dns_parse_response(const uint8_t *buf, size_t len, uint32_t *out_ip);

/* Query DNS server for hostname resolution */
int dns_resolve(net_if_t *netif, const char *hostname, uint32_t *out_ip);
