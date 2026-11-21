/* ZERO KERNEL - User Datagram Protocol (UDP) (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 768 UDP datagrams, port demultiplexing, and transmission.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

typedef struct __attribute__((packed)) udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;    /* Header + payload length (bytes) */
    uint16_t checksum;
} udp_hdr_t;

/* Process incoming UDP datagram */
int udp_rx(net_if_t *netif, net_buf_t *buf, uint32_t src_ip, uint32_t dst_ip);

/* Transmit UDP datagram */
int udp_tx(net_if_t *netif, net_buf_t *buf, uint16_t src_port, uint32_t dst_ip, uint16_t dst_port);
