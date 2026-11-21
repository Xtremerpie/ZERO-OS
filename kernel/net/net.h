/* ZERO KERNEL - Central Network Subsystem & Interface Abstraction (Milestone 4.0.0 Storage & Networking).
 *
 * Defines network interface structures (net_if_t), endian conversion routines,
 * device registration, packet ingress/egress dispatch, and traffic statistics.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net_buf.h"

#define NET_MAX_INTERFACES   8u
#define NET_ETH_ALEN         6u
#define NET_DEFAULT_MTU      1500u

#define NETIF_FLAG_UP        0x01u
#define NETIF_FLAG_BROADCAST 0x02u
#define NETIF_FLAG_LINK_UP   0x04u
#define NETIF_FLAG_DHCP      0x08u

/* Big-Endian / Network-Byte-Order conversion helpers */
static inline uint16_t htons(uint16_t v) {
    return (uint16_t)(((v & 0xFFu) << 8) | ((v >> 8) & 0xFFu));
}
static inline uint16_t ntohs(uint16_t v) {
    return htons(v);
}
static inline uint32_t htonl(uint32_t v) {
    return (((v & 0x000000FFu) << 24) |
            ((v & 0x0000FF00u) << 8)  |
            ((v & 0x00FF0000u) >> 8)  |
            ((v & 0xFF000000u) >> 24));
}
static inline uint32_t ntohl(uint32_t v) {
    return htonl(v);
}

/* Helper to construct IPv4 address from 4 octets */
static inline uint32_t IP4_ADDR(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

struct net_if;

typedef struct net_if {
    char     name[16];
    uint8_t  mac[NET_ETH_ALEN];
    uint32_t ip;      /* Network byte order */
    uint32_t netmask; /* Network byte order */
    uint32_t gateway; /* Network byte order */
    uint32_t dns;     /* Network byte order */
    uint32_t mtu;
    uint32_t flags;

    /* Traffic counters */
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;

    /* Device operations */
    int  (*send)(struct net_if *netif, net_buf_t *buf);
    void (*poll)(struct net_if *netif);

    void *driver_data;
    struct net_if *next;
} net_if_t;

/* Initialize central network subsystem */
void net_init(void);

/* Register a network interface */
int net_if_register(net_if_t *netif);

/* Get default network interface (e.g. eth0) */
net_if_t *net_if_get_default(void);

/* Find network interface by name */
net_if_t *net_if_get_by_name(const char *name);

/* Retrieve total number of registered interfaces */
size_t net_if_count(void);

/* Get network interface by index */
net_if_t *net_if_get(size_t index);

/* Ingress packet entry point from drivers */
int net_rx(net_if_t *netif, net_buf_t *buf);

/* Egress packet entry point to driver */
int net_tx(net_if_t *netif, net_buf_t *buf);

/* Poll all registered interfaces for incoming packets */
void net_poll_all(void);
