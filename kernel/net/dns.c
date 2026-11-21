/* ZERO KERNEL - Domain Name System (DNS) Resolver Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements RFC 1035 hostname query building, QNAME label compression/encoding, and answer parsing.
 */
#include "net/dns.h"
#include "net/udp.h"
#include "zero/log.h"
#include "lib/string.h"

int dns_build_query(const char *hostname, uint16_t tx_id, uint8_t *out_buf, size_t max_len)
{
    if (!hostname || !out_buf || max_len < (sizeof(dns_hdr_t) + strlen(hostname) + 6)) {
        return -1;
    }

    dns_hdr_t *hdr = (dns_hdr_t *)out_buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->id = htons(tx_id);
    hdr->flags = htons(DNS_FLAG_QUERY);
    hdr->qdcount = htons(1);

    uint8_t *p = out_buf + sizeof(dns_hdr_t);
    const char *src = hostname;

    /* QNAME encoding: "www.google.com" -> 3www6google3com0 */
    while (*src) {
        const char *dot = strchr(src, '.');
        size_t label_len = dot ? (size_t)(dot - src) : strlen(src);
        if (label_len > 63) return -2;

        *p++ = (uint8_t)label_len;
        for (size_t i = 0; i < label_len; i++) {
            *p++ = (uint8_t)src[i];
        }

        if (!dot) break;
        src = dot + 1;
    }
    *p++ = 0; /* Null terminator for QNAME */

    /* QTYPE (A = 1) */
    *p++ = 0;
    *p++ = DNS_TYPE_A;

    /* QCLASS (IN = 1) */
    *p++ = 0;
    *p++ = DNS_CLASS_IN;

    return (int)(p - out_buf);
}

int dns_parse_response(const uint8_t *buf, size_t len, uint32_t *out_ip)
{
    if (!buf || !out_ip || len < sizeof(dns_hdr_t)) {
        return -1;
    }

    const dns_hdr_t *hdr = (const dns_hdr_t *)buf;
    uint16_t ancount = ntohs(hdr->ancount);
    if (ancount == 0) {
        return -2; /* No answers returned */
    }

    const uint8_t *p = buf + sizeof(dns_hdr_t);
    const uint8_t *end = buf + len;

    /* Skip Question Section */
    uint16_t qdcount = ntohs(hdr->qdcount);
    for (uint16_t q = 0; q < qdcount; q++) {
        while (p < end && *p != 0) {
            if ((*p & 0xC0) == 0xC0) {
                p += 2;
                break;
            }
            p += (*p + 1);
        }
        if (p < end && *p == 0) p++;
        p += 4; /* Skip QTYPE and QCLASS */
    }

    /* Parse First Answer Record */
    for (uint16_t a = 0; a < ancount && p < end; a++) {
        /* Skip NAME field */
        if ((*p & 0xC0) == 0xC0) {
            p += 2; /* Pointer */
        } else {
            while (p < end && *p != 0) {
                p += (*p + 1);
            }
            if (p < end && *p == 0) p++;
        }

        if (p + 10 > end) return -3;

        uint16_t type = (uint16_t)((p[0] << 8) | p[1]);
        uint16_t rdlen = (uint16_t)((p[8] << 8) | p[9]);
        p += 10; /* Skip TYPE(2), CLASS(2), TTL(4), RDLENGTH(2) */

        if (type == DNS_TYPE_A && rdlen == 4) {
            if (p + 4 > end) return -4;
            memcpy(out_ip, p, 4);
            return 0;
        }

        p += rdlen;
    }

    return -5;
}

int dns_resolve(net_if_t *netif, const char *hostname, uint32_t *out_ip)
{
    if (!netif || !hostname || !out_ip || netif->dns == 0) {
        return -1;
    }

    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM + sizeof(udp_hdr_t) + sizeof(ipv4_hdr_t));
    if (!buf) return -2;

    uint8_t qbuf[256];
    int qlen = dns_build_query(hostname, 0x1A2B, qbuf, sizeof(qbuf));
    if (qlen <= 0) {
        net_buf_free(buf);
        return -3;
    }

    void *p = net_buf_put(buf, (size_t)qlen);
    if (!p) {
        net_buf_free(buf);
        return -4;
    }
    memcpy(p, qbuf, (size_t)qlen);

    return udp_tx(netif, buf, 54321, netif->dns, DNS_PORT);
}
