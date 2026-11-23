/* ZERO KERNEL - Transmission Control Protocol (TCP) Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 793 connection management, 3-way handshake, state machine, and flow buffers.
 */
#include "net/tcp.h"
#include "net/ipv4.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "zero/string.h"

static tcp_pcb_t   g_tcp_pcb_pool[TCP_MAX_CONNS];
static spinlock_t  g_tcp_lock;

void tcp_init(void)
{
    spinlock_init(&g_tcp_lock);
    spin_lock(&g_tcp_lock);
    memset(g_tcp_pcb_pool, 0, sizeof(g_tcp_pcb_pool));
    spin_unlock(&g_tcp_lock);
    KLOG_INFO("tcp", "initialized TCP protocol engine (%u concurrent connections)", TCP_MAX_CONNS);
}

tcp_pcb_t *tcp_alloc(void)
{
    spin_lock(&g_tcp_lock);
    for (size_t i = 0; i < TCP_MAX_CONNS; i++) {
        if (!g_tcp_pcb_pool[i].in_use) {
            tcp_pcb_t *pcb = &g_tcp_pcb_pool[i];
            memset(pcb, 0, sizeof(*pcb));
            pcb->in_use = true;
            pcb->state = TCP_STATE_CLOSED;
            pcb->snd_wnd = TCP_DEFAULT_WINDOW;
            pcb->rcv_wnd = TCP_DEFAULT_WINDOW;
            spin_unlock(&g_tcp_lock);
            return pcb;
        }
    }
    spin_unlock(&g_tcp_lock);
    return NULL;
}

void tcp_free(tcp_pcb_t *pcb)
{
    if (!pcb) return;
    spin_lock(&g_tcp_lock);
    pcb->in_use = false;
    pcb->state = TCP_STATE_CLOSED;
    spin_unlock(&g_tcp_lock);
}

uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const tcp_hdr_t *hdr,
                      size_t tcp_len, const void *payload, size_t payload_len)
{
    struct __attribute__((packed)) {
        uint32_t src;
        uint32_t dst;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t len;
    } pseudo;

    pseudo.src = src_ip;
    pseudo.dst = dst_ip;
    pseudo.zero = 0;
    pseudo.proto = IPV4_PROTO_TCP;
    pseudo.len = htons((uint16_t)tcp_len);

    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)&pseudo;
    for (size_t i = 0; i < sizeof(pseudo) / 2; i++) {
        sum += p[i];
    }

    p = (const uint16_t *)hdr;
    for (size_t i = 0; i < sizeof(tcp_hdr_t) / 2; i++) {
        sum += p[i];
    }

    if (payload && payload_len > 0) {
        p = (const uint16_t *)payload;
        size_t pl = payload_len;
        while (pl > 1) {
            sum += *p++;
            pl -= 2;
        }
        if (pl == 1) {
            sum += *(const uint8_t *)p;
        }
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static int tcp_send_segment(net_if_t *netif, tcp_pcb_t *pcb, uint8_t flags,
                            const void *payload, size_t payload_len)
{
    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM + sizeof(tcp_hdr_t) + sizeof(ipv4_hdr_t));
    if (!buf) return -1;

    if (payload && payload_len > 0) {
        void *p = net_buf_put(buf, payload_len);
        if (!p) {
            net_buf_free(buf);
            return -2;
        }
        memcpy(p, payload, payload_len);
    }

    tcp_hdr_t *hdr = (tcp_hdr_t *)net_buf_push(buf, sizeof(tcp_hdr_t));
    if (!hdr) {
        net_buf_free(buf);
        return -3;
    }

    hdr->src_port = htons(pcb->local_port);
    hdr->dst_port = htons(pcb->remote_port);
    hdr->seq_num = htonl(pcb->snd_nxt);
    hdr->ack_num = htonl(pcb->rcv_nxt);
    hdr->data_offset = (5u << 4); /* 5 32-bit words = 20 bytes */
    hdr->flags = flags;
    hdr->window_size = htons(pcb->rcv_wnd);
    hdr->urgent_ptr = 0;
    hdr->checksum = 0;

    hdr->checksum = tcp_checksum(pcb->local_ip ? pcb->local_ip : netif->ip,
                                 pcb->remote_ip, hdr, buf->len, payload, payload_len);

    return ipv4_tx(netif, buf, pcb->remote_ip, IPV4_PROTO_TCP);
}

int tcp_bind(tcp_pcb_t *pcb, uint32_t local_ip, uint16_t local_port)
{
    if (!pcb) return -1;
    pcb->local_ip = local_ip;
    pcb->local_port = local_port;
    return 0;
}

int tcp_listen(tcp_pcb_t *pcb)
{
    if (!pcb) return -1;
    pcb->state = TCP_STATE_LISTEN;
    return 0;
}

int tcp_connect(tcp_pcb_t *pcb, uint32_t remote_ip, uint16_t remote_port)
{
    if (!pcb) return -1;

    net_if_t *netif = net_if_get_default();
    if (!netif) return -2;

    pcb->local_ip = netif->ip;
    if (pcb->local_port == 0) {
        pcb->local_port = 49152; /* Ephemeral port */
    }
    pcb->remote_ip = remote_ip;
    pcb->remote_port = remote_port;
    pcb->snd_nxt = 1000;
    pcb->rcv_nxt = 0;

    pcb->state = TCP_STATE_SYN_SENT;
    int rc = tcp_send_segment(netif, pcb, TCP_FLAG_SYN, NULL, 0);
    pcb->snd_nxt++;
    return rc;
}

int tcp_send(tcp_pcb_t *pcb, const void *data, size_t len)
{
    if (!pcb || !data || pcb->state != TCP_STATE_ESTABLISHED) {
        return -1;
    }

    net_if_t *netif = net_if_get_default();
    if (!netif) return -2;

    int rc = tcp_send_segment(netif, pcb, TCP_FLAG_ACK | TCP_FLAG_PSH, data, len);
    if (rc == 0) {
        pcb->snd_nxt += (uint32_t)len;
    }
    return rc;
}

int tcp_recv(tcp_pcb_t *pcb, void *data, size_t max_len)
{
    if (!pcb || !data || max_len == 0) return 0;

    spin_lock(&g_tcp_lock);
    size_t to_read = pcb->rx_len;
    if (to_read > max_len) to_read = max_len;

    if (to_read > 0) {
        for (size_t i = 0; i < to_read; i++) {
            ((uint8_t *)data)[i] = pcb->rx_buf[(pcb->rx_head + i) % TCP_BUFFER_SIZE];
        }
        pcb->rx_head = (pcb->rx_head + (uint32_t)to_read) % TCP_BUFFER_SIZE;
        pcb->rx_len -= (uint32_t)to_read;
    }
    spin_unlock(&g_tcp_lock);

    return (int)to_read;
}

int tcp_close(tcp_pcb_t *pcb)
{
    if (!pcb) return -1;

    net_if_t *netif = net_if_get_default();
    if (netif && (pcb->state == TCP_STATE_ESTABLISHED || pcb->state == TCP_STATE_CLOSE_WAIT)) {
        tcp_send_segment(netif, pcb, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        pcb->snd_nxt++;
        if (pcb->state == TCP_STATE_ESTABLISHED) {
            pcb->state = TCP_STATE_FIN_WAIT_1;
        } else {
            pcb->state = TCP_STATE_LAST_ACK;
        }
    } else {
        pcb->state = TCP_STATE_CLOSED;
    }
    return 0;
}

int tcp_rx(net_if_t *netif, net_buf_t *buf, uint32_t src_ip, uint32_t dst_ip)
{
    (void)dst_ip;
    if (!netif || !buf || buf->len < sizeof(tcp_hdr_t)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    const tcp_hdr_t *hdr = (const tcp_hdr_t *)buf->data;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint32_t seq = ntohl(hdr->seq_num);
    uint32_t ack = ntohl(hdr->ack_num);
    uint8_t  flags = hdr->flags;

    uint8_t offset_words = (hdr->data_offset >> 4);
    size_t  tcp_hdr_len = offset_words * 4;
    if (buf->len < tcp_hdr_len) {
        net_buf_free(buf);
        return -2;
    }

    size_t payload_len = buf->len - tcp_hdr_len;
    const uint8_t *payload = buf->data + tcp_hdr_len;

    spin_lock(&g_tcp_lock);
    tcp_pcb_t *pcb = NULL;

    /* Match connection */
    for (size_t i = 0; i < TCP_MAX_CONNS; i++) {
        tcp_pcb_t *p = &g_tcp_pcb_pool[i];
        if (p->in_use && p->local_port == dst_port) {
            if (p->remote_port == src_port && p->remote_ip == src_ip) {
                pcb = p;
                break;
            } else if (p->state == TCP_STATE_LISTEN) {
                pcb = p; /* Found listening socket */
            }
        }
    }

    if (!pcb) {
        spin_unlock(&g_tcp_lock);
        net_buf_free(buf);
        return 0;
    }

    /* Process state transitions */
    switch (pcb->state) {
        case TCP_STATE_LISTEN:
            if (flags & TCP_FLAG_SYN) {
                pcb->remote_ip = src_ip;
                pcb->remote_port = src_port;
                pcb->rcv_nxt = seq + 1;
                pcb->snd_nxt = 2000;
                pcb->state = TCP_STATE_SYN_RECEIVED;
                tcp_send_segment(netif, pcb, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                pcb->snd_nxt++;
            }
            break;

        case TCP_STATE_SYN_SENT:
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                pcb->rcv_nxt = seq + 1;
                pcb->snd_una = ack;
                pcb->state = TCP_STATE_ESTABLISHED;
                tcp_send_segment(netif, pcb, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_STATE_SYN_RECEIVED:
            if (flags & TCP_FLAG_ACK) {
                pcb->snd_una = ack;
                pcb->state = TCP_STATE_ESTABLISHED;
            }
            break;

        case TCP_STATE_ESTABLISHED:
            if (payload_len > 0) {
                for (size_t i = 0; i < payload_len; i++) {
                    if (pcb->rx_len < TCP_BUFFER_SIZE) {
                        pcb->rx_buf[pcb->rx_tail] = payload[i];
                        pcb->rx_tail = (pcb->rx_tail + 1) % TCP_BUFFER_SIZE;
                        pcb->rx_len++;
                    }
                }
                pcb->rcv_nxt += (uint32_t)payload_len;
                tcp_send_segment(netif, pcb, TCP_FLAG_ACK, NULL, 0);
            }
            if (flags & TCP_FLAG_FIN) {
                pcb->rcv_nxt++;
                pcb->state = TCP_STATE_CLOSE_WAIT;
                tcp_send_segment(netif, pcb, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_STATE_FIN_WAIT_1:
            if (flags & TCP_FLAG_ACK) {
                pcb->state = TCP_STATE_FIN_WAIT_2;
            }
            if (flags & TCP_FLAG_FIN) {
                pcb->rcv_nxt++;
                tcp_send_segment(netif, pcb, TCP_FLAG_ACK, NULL, 0);
                pcb->state = TCP_STATE_TIME_WAIT;
            }
            break;

        case TCP_STATE_FIN_WAIT_2:
            if (flags & TCP_FLAG_FIN) {
                pcb->rcv_nxt++;
                tcp_send_segment(netif, pcb, TCP_FLAG_ACK, NULL, 0);
                pcb->state = TCP_STATE_TIME_WAIT;
            }
            break;

        default:
            break;
    }

    spin_unlock(&g_tcp_lock);
    net_buf_free(buf);
    return 0;
}
