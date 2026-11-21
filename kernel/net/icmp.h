/* ZERO KERNEL - Internet Control Message Protocol (ICMP) (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 792 ICMP Echo Request/Reply (ping) handling and generation.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define ICMP_TYPE_ECHO_REPLY    0u
#define ICMP_TYPE_ECHO_REQUEST  8u

typedef struct __attribute__((packed)) icmp_hdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_hdr_t;

/* Process incoming ICMP packet */
int icmp_rx(net_if_t *netif, net_buf_t *buf, uint32_t src_ip);

/* Send an ICMP Echo Request (ping) */
int icmp_send_echo(net_if_t *netif, uint32_t dst_ip, uint16_t id, uint16_t seq,
                   const void *payload, size_t payload_len);
