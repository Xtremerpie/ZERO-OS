/* ZERO KERNEL - User Datagram Protocol (UDP) Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 768 UDP multiplexing, port dispatching, and socket delivery.
 */
#include "net/udp.h"
#include "net/ipv4.h"
#include "net/dhcp.h"
#include "net/socket.h"
#include "zero/log.h"
#include "lib/string.h"

int udp_rx(net_if_t *netif, net_buf_t *buf, uint32_t src_ip, uint32_t dst_ip)
{
    (void)src_ip;
    (void)dst_ip;
    if (!netif || !buf || buf->len < sizeof(udp_hdr_t)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    const udp_hdr_t *hdr = (const udp_hdr_t *)buf->data;
    uint16_t dst_port = ntohs(hdr->dst_port);

    /* Strip UDP header */
    net_buf_pull(buf, sizeof(udp_hdr_t));

    /* Check well-known protocol ports */
    if (dst_port == DHCP_CLIENT_PORT) {
        return dhcp_rx(netif, buf);
    }

    /* Check registered socket listeners */
    for (size_t i = 0; i < SOCKET_MAX_DESCRIPTORS; i++) {
        zero_socket_t *s = socket_get((int)i);
        if (s && s->in_use && s->type == SOCK_DGRAM && s->local_port == dst_port) {
            size_t to_copy = buf->len;
            if (to_copy > sizeof(s->dgram_buf)) to_copy = sizeof(s->dgram_buf);
            memcpy(s->dgram_buf, buf->data, to_copy);
            s->dgram_len = to_copy;
            s->remote_ip = src_ip;
            s->remote_port = ntohs(hdr->src_port);
            net_buf_free(buf);
            return 0;
        }
    }

    net_buf_free(buf);
    return 0;
}

int udp_tx(net_if_t *netif, net_buf_t *buf, uint16_t src_port, uint32_t dst_ip, uint16_t dst_port)
{
    if (!netif || !buf) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    udp_hdr_t *hdr = (udp_hdr_t *)net_buf_push(buf, sizeof(udp_hdr_t));
    if (!hdr) {
        net_buf_free(buf);
        return -2;
    }

    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length = htons((uint16_t)buf->len);
    hdr->checksum = 0; /* Optional in IPv4 */

    return ipv4_tx(netif, buf, dst_ip, IPV4_PROTO_UDP);
}
