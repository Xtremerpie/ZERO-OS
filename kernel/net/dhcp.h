/* ZERO KERNEL - Dynamic Host Configuration Protocol (DHCP) Client (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 2131 DHCP client state machine (INIT, DISCOVERING, REQUESTING, BOUND),
 * option parsing (Subnet Mask, Router/Gateway, DNS), and interface auto-configuration.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define DHCP_BOOTREQUEST       1u
#define DHCP_BOOTREPLY         2u
#define DHCP_MAGIC_COOKIE      0x63825363u

#define DHCP_SERVER_PORT       67u
#define DHCP_CLIENT_PORT       68u

#define DHCP_MSG_DISCOVER      1u
#define DHCP_MSG_OFFER         2u
#define DHCP_MSG_REQUEST       3u
#define DHCP_MSG_DECLINE       4u
#define DHCP_MSG_ACK           5u
#define DHCP_MSG_NAK           6u

#define DHCP_OPT_PAD           0u
#define DHCP_OPT_SUBNET_MASK   1u
#define DHCP_OPT_ROUTER        3u
#define DHCP_OPT_DNS_SERVER    6u
#define DHCP_OPT_REQUESTED_IP  50u
#define DHCP_OPT_MSG_TYPE      53u
#define DHCP_OPT_SERVER_ID     54u
#define DHCP_OPT_END           255u

typedef enum dhcp_state {
    DHCP_STATE_INIT = 0,
    DHCP_STATE_DISCOVERING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
} dhcp_state_t;

typedef struct __attribute__((packed)) dhcp_msg {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    char     sname[64];
    char     file[128];
    uint32_t cookie;
    uint8_t  options[308];
} dhcp_msg_t;

/* Start DHCP configuration for interface */
int dhcp_start(net_if_t *netif);

/* Process incoming DHCP packet on port 68 */
int dhcp_rx(net_if_t *netif, net_buf_t *buf);

/* Get current DHCP state */
dhcp_state_t dhcp_get_state(void);
