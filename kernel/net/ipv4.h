/* ZERO KERNEL - Internet Protocol Version 4 (IPv4) (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 791 IPv4 packet parsing, checksum validation, route selection,
 * and protocol dispatch (ICMP, TCP, UDP).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define IPV4_VERSION      4u
#define IPV4_MIN_HLEN     20u

#define IPV4_PROTO_ICMP   1u
#define IPV4_PROTO_TCP    6u
#define IPV4_PROTO_UDP    17u

#define IPV4_FLAG_DF      0x4000u /* Don't Fragment */
#define IPV4_DEFAULT_TTL  64u

typedef struct __attribute__((packed)) ipv4_hdr {
    uint8_t  version_ihl;   /* (version << 4) | (ihl) */
    uint8_t  dscp_ecn;
    uint16_t total_length;  /* Header + data in bytes (big endian) */
    uint16_t id;
    uint16_t flags_frag;    /* Flags + Fragment offset */
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;      /* Header checksum (big endian) */
    uint32_t src_ip;        /* Network byte order */
    uint32_t dst_ip;        /* Network byte order */
} ipv4_hdr_t;

/* Compute RFC 791 16-bit Internet checksum */
uint16_t ipv4_checksum(const void *data, size_t length);

/* Process incoming IPv4 packet */
int ipv4_rx(net_if_t *netif, net_buf_t *buf);

/* Encapsulate packet with IPv4 header and transmit */
int ipv4_tx(net_if_t *netif, net_buf_t *buf, uint32_t dst_ip, uint8_t protocol);
