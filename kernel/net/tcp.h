/* ZERO KERNEL - Transmission Control Protocol (TCP) (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 793 TCP state machine, segment framing, 3-way handshake,
 * sequence/acknowledgement tracking, checksumming, and flow buffers.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"
#include "net/net_buf.h"

#define TCP_FLAG_FIN    0x01u
#define TCP_FLAG_SYN    0x02u
#define TCP_FLAG_RST    0x04u
#define TCP_FLAG_PSH    0x08u
#define TCP_FLAG_ACK    0x10u
#define TCP_FLAG_URG    0x20u

#define TCP_DEFAULT_WINDOW 8192u
#define TCP_BUFFER_SIZE    4096u
#define TCP_MAX_CONNS      16u

typedef enum tcp_state {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT,
} tcp_state_t;

typedef struct __attribute__((packed)) tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset; /* (offset_words << 4) */
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} tcp_hdr_t;

/* TCP Protocol Control Block (PCB) */
typedef struct tcp_pcb {
    uint32_t    local_ip;
    uint16_t    local_port;
    uint32_t    remote_ip;
    uint16_t    remote_port;
    tcp_state_t state;

    uint32_t    snd_una; /* Send unacknowledged */
    uint32_t    snd_nxt; /* Next send sequence */
    uint32_t    rcv_nxt; /* Next receive expected sequence */
    uint16_t    snd_wnd;
    uint16_t    rcv_wnd;

    /* Receive buffer */
    uint8_t     rx_buf[TCP_BUFFER_SIZE];
    uint32_t    rx_head;
    uint32_t    rx_tail;
    uint32_t    rx_len;

    bool        in_use;
    struct tcp_pcb *next;
} tcp_pcb_t;

/* Initialize TCP subsystem */
void tcp_init(void);

/* Allocate a new TCP PCB */
tcp_pcb_t *tcp_alloc(void);

/* Free a TCP PCB */
void tcp_free(tcp_pcb_t *pcb);

/* Bind PCB to local address and port */
int tcp_bind(tcp_pcb_t *pcb, uint32_t local_ip, uint16_t local_port);

/* Transition PCB to LISTEN state */
int tcp_listen(tcp_pcb_t *pcb);

/* Initiate active connection (sends SYN) */
int tcp_connect(tcp_pcb_t *pcb, uint32_t remote_ip, uint16_t remote_port);

/* Send data over an established TCP connection */
int tcp_send(tcp_pcb_t *pcb, const void *data, size_t len);

/* Read received data from connection buffer */
int tcp_recv(tcp_pcb_t *pcb, void *data, size_t max_len);

/* Close connection (initiates FIN handshake) */
int tcp_close(tcp_pcb_t *pcb);

/* Process incoming TCP segment */
int tcp_rx(net_if_t *netif, net_buf_t *buf, uint32_t src_ip, uint32_t dst_ip);

/* Calculate TCP checksum with IPv4 pseudo-header */
uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const tcp_hdr_t *hdr,
                      size_t tcp_len, const void *payload, size_t payload_len);
