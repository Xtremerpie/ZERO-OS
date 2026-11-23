/* ZERO KERNEL - Internet Control Message Protocol (ICMP) Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 792 Echo Request/Echo Reply processing and ping generation.
 */
#include "net/icmp.h"
#include "net/ipv4.h"
#include "zero/log.h"
#include "zero/string.h"

int icmp_rx(net_if_t *netif, net_buf_t *buf, uint32_t src_ip)
{
    if (!netif || !buf || buf->len < sizeof(icmp_hdr_t)) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    icmp_hdr_t *hdr = (icmp_hdr_t *)buf->data;

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        /* Immediate RFC 792 Echo Reply */
        hdr->type = ICMP_TYPE_ECHO_REPLY;
        hdr->code = 0;
        hdr->checksum = 0;
        hdr->checksum = ipv4_checksum(buf->data, buf->len);

        return ipv4_tx(netif, buf, src_ip, IPV4_PROTO_ICMP);
    }

    net_buf_free(buf);
    return 0;
}

int icmp_send_echo(net_if_t *netif, uint32_t dst_ip, uint16_t id, uint16_t seq,
                   const void *payload, size_t payload_len)
{
    if (!netif) return -1;

    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM + sizeof(ipv4_hdr_t) + sizeof(icmp_hdr_t));
    if (!buf) return -2;

    if (payload && payload_len > 0) {
        void *p = net_buf_put(buf, payload_len);
        if (!p) {
            net_buf_free(buf);
            return -3;
        }
        memcpy(p, payload, payload_len);
    }

    icmp_hdr_t *hdr = (icmp_hdr_t *)net_buf_push(buf, sizeof(icmp_hdr_t));
    if (!hdr) {
        net_buf_free(buf);
        return -4;
    }

    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->id = htons(id);
    hdr->seq = htons(seq);
    hdr->checksum = 0;
    hdr->checksum = ipv4_checksum(buf->data, buf->len);

    return ipv4_tx(netif, buf, dst_ip, IPV4_PROTO_ICMP);
}
