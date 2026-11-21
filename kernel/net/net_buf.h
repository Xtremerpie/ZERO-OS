/* ZERO KERNEL - Network Packet Buffer Subsystem (Milestone 4.0.0 Storage & Networking).
 *
 * Implements high-performance reference-counted packet buffers with
 * zero-copy headroom reservation, prepend (push), and strip (pull) operations.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define NET_BUF_SIZE        2048u
#define NET_BUF_HEADROOM    128u
#define NET_BUF_POOL_SIZE   64u

typedef struct net_buf {
    uint8_t  buffer[NET_BUF_SIZE];
    uint8_t *head;        /* Start of allocated memory buffer */
    uint8_t *data;        /* Start of actual packet payload */
    uint8_t *tail;        /* End of actual packet payload */
    uint8_t *end;         /* End of allocated memory buffer */
    uint32_t len;         /* Current payload length (tail - data) */
    uint32_t ref_count;
    bool     in_use;
    struct net_buf *next; /* For queueing */
} net_buf_t;

/* Initialize packet buffer pool */
void net_buf_init(void);

/* Allocate packet buffer with standard or custom headroom */
net_buf_t *net_buf_alloc(size_t reserve_headroom);

/* Free packet buffer back to pool */
void net_buf_free(net_buf_t *buf);

/* Prepend len bytes to payload (moves data pointer back) */
void *net_buf_push(net_buf_t *buf, size_t len);

/* Strip len bytes from start of payload (advances data pointer) */
void *net_buf_pull(net_buf_t *buf, size_t len);

/* Append len bytes to end of payload (advances tail pointer) */
void *net_buf_put(net_buf_t *buf, size_t len);

/* Reserve initial headroom before adding payload */
void net_buf_reserve(net_buf_t *buf, size_t len);

/* Retrieve count of available buffers in pool */
size_t net_buf_available(void);
