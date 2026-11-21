/* ZERO KERNEL - Intel 82540EM (E1000) Gigabit Ethernet Driver (Milestone 4.0.0 Storage & Networking).
 *
 * Implements hardware discovery, MMIO register mapping, descriptor rings,
 * packet transmission, and interrupt/poll reception.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"

#define E1000_VENDOR_INTEL     0x8086u
#define E1000_DEV_82540EM      0x100Eu
#define E1000_DEV_82543GC      0x1004u
#define E1000_DEV_82545EM      0x100Fu

#define E1000_NUM_RX_DESC      32u
#define E1000_NUM_TX_DESC      32u

/* Initialize Intel E1000 network driver and probe PCI bus */
bool e1000_init(void);

/* Transmit packet over E1000 hardware */
int e1000_send_packet(net_if_t *netif, net_buf_t *buf);

/* Poll E1000 RX ring for incoming packets */
void e1000_poll_rx(net_if_t *netif);

/* Check if E1000 hardware was detected and initialized */
bool e1000_is_present(void);
