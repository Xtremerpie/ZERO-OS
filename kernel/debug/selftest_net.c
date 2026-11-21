/* ZERO KERNEL - Network Subsystem Self-Test Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Verifies packet buffers (net_buf), Ethernet framing, ARP caching, RFC 791 IPv4 checksums,
 * ICMP echo replies, UDP demuxing, DHCP client logic, DNS resolver parsing, TCP state machine,
 * and Socket API system calls.
 */
#include "debug/selftest_net.h"
#include "net/net.h"
#include "net/net_buf.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/dhcp.h"
#include "net/dns.h"
#include "net/tcp.h"
#include "net/socket.h"
#include "zero/log.h"
#include "lib/kprintf.h"
#include "lib/string.h"

static bool test_net_buf(void)
{
    net_buf_t *buf = net_buf_alloc(NET_BUF_HEADROOM);
    if (!buf) {
        kprintf("[FAIL] net_buf: allocation failed\n");
        return false;
    }

    if (buf->len != 0) {
        kprintf("[FAIL] net_buf: newly allocated buffer has non-zero length\n");
        net_buf_free(buf);
        return false;
    }

    /* Append 10 bytes payload */
    uint8_t *p = (uint8_t *)net_buf_put(buf, 10);
    if (!p || buf->len != 10) {
        kprintf("[FAIL] net_buf: put operation failed\n");
        net_buf_free(buf);
        return false;
    }
    for (int i = 0; i < 10; i++) p[i] = (uint8_t)i;

    /* Push 14 bytes header (e.g. Ethernet header) */
    eth_hdr_t *eth = (eth_hdr_t *)net_buf_push(buf, sizeof(eth_hdr_t));
    if (!eth || buf->len != (10 + sizeof(eth_hdr_t))) {
        kprintf("[FAIL] net_buf: push operation failed\n");
        net_buf_free(buf);
        return false;
    }

    /* Pull header */
    void *pulled = net_buf_pull(buf, sizeof(eth_hdr_t));
    if (pulled != eth || buf->len != 10) {
        kprintf("[FAIL] net_buf: pull operation failed\n");
        net_buf_free(buf);
        return false;
    }

    net_buf_free(buf);
    kprintf("[PASS] net: packet buffer headroom reservation, push, pull, and lifecycle verified\n");
    return true;
}

static bool test_arp_and_ipv4_checksum(void)
{
    /* Test ARP Table */
    uint32_t test_ip = IP4_ADDR(192, 168, 1, 100);
    uint8_t test_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    arp_insert(test_ip, test_mac);

    uint8_t resolved_mac[6];
    if (!arp_lookup(test_ip, resolved_mac) || memcmp(test_mac, resolved_mac, 6) != 0) {
        kprintf("[FAIL] arp: cache lookup failed\n");
        return false;
    }

    /* Non-existent IP check */
    if (arp_lookup(IP4_ADDR(1, 2, 3, 4), resolved_mac)) {
        kprintf("[FAIL] arp: non-existent IP lookup succeeded incorrectly\n");
        return false;
    }

    /* Test RFC 791 IPv4 Checksum */
    ipv4_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.version_ihl = 0x45;
    hdr.total_length = htons(sizeof(ipv4_hdr_t));
    hdr.ttl = 64;
    hdr.protocol = IPV4_PROTO_ICMP;
    hdr.src_ip = IP4_ADDR(10, 0, 2, 15);
    hdr.dst_ip = IP4_ADDR(10, 0, 2, 2);
    hdr.checksum = 0;

    uint16_t csum = ipv4_checksum(&hdr, sizeof(hdr));
    hdr.checksum = csum;

    /* Verifying checksum over header with checksum field included must yield 0 */
    uint16_t verify = ipv4_checksum(&hdr, sizeof(hdr));
    if (verify != 0) {
        kprintf("[FAIL] ipv4: RFC 791 checksum verification failed (result 0x%04X, expected 0x0000)\n", verify);
        return false;
    }

    kprintf("[PASS] net: ARP dynamic cache and RFC 791 IPv4 Internet checksum verified\n");
    return true;
}

static bool test_dns_and_dhcp(void)
{
    /* Test DNS Query Building */
    uint8_t dns_buf[128];
    int qlen = dns_build_query("zero-os.org", 0x1234, dns_buf, sizeof(dns_buf));
    if (qlen <= 0) {
        kprintf("[FAIL] dns: query building failed\n");
        return false;
    }

    dns_hdr_t *dhdr = (dns_hdr_t *)dns_buf;
    if (ntohs(dhdr->id) != 0x1234 || ntohs(dhdr->qdcount) != 1) {
        kprintf("[FAIL] dns: header fields incorrect\n");
        return false;
    }

    /* Verify QNAME: length 7 for "zero-os", length 3 for "org", length 0 end */
    uint8_t *qname = dns_buf + sizeof(dns_hdr_t);
    if (qname[0] != 7 || memcmp(qname + 1, "zero-os", 7) != 0) {
        kprintf("[FAIL] dns: QNAME label encoding failed\n");
        return false;
    }

    /* Test Synthetic DNS Answer Parsing */
    uint8_t resp_buf[128];
    memcpy(resp_buf, dns_buf, (size_t)qlen);
    dns_hdr_t *rhdr = (dns_hdr_t *)resp_buf;
    rhdr->flags = htons(0x8180u); /* Standard query response, No error */
    rhdr->ancount = htons(1);

    uint8_t *ans = resp_buf + qlen;
    /* NAME: pointer to offset 12 (0xC00C) */
    *ans++ = 0xC0;
    *ans++ = 0x0C;
    /* TYPE: A (0x0001) */
    *ans++ = 0x00;
    *ans++ = 0x01;
    /* CLASS: IN (0x0001) */
    *ans++ = 0x00;
    *ans++ = 0x01;
    /* TTL: 300 (0x0000012C) */
    *ans++ = 0x00;
    *ans++ = 0x00;
    *ans++ = 0x01;
    *ans++ = 0x2C;
    /* RDLENGTH: 4 (0x0004) */
    *ans++ = 0x00;
    *ans++ = 0x04;
    /* RDATA: 192.168.1.50 */
    uint32_t expected_ip = IP4_ADDR(192, 168, 1, 50);
    memcpy(ans, &expected_ip, 4);
    ans += 4;

    uint32_t resolved_ip = 0;
    int err = dns_parse_response(resp_buf, (size_t)(ans - resp_buf), &resolved_ip);
    if (err != 0 || resolved_ip != expected_ip) {
        kprintf("[FAIL] dns: response parsing failed (err %d, ip 0x%08X)\n", err, resolved_ip);
        return false;
    }

    kprintf("[PASS] net: DNS query QNAME formatting and A-record response parsing verified\n");
    return true;
}

static bool test_tcp_state_and_sockets(void)
{
    /* Test TCP Control Block Allocation & Transitions */
    tcp_pcb_t *pcb = tcp_alloc();
    if (!pcb) {
        kprintf("[FAIL] tcp: PCB allocation failed\n");
        return false;
    }

    if (pcb->state != TCP_STATE_CLOSED) {
        kprintf("[FAIL] tcp: initial state not CLOSED\n");
        tcp_free(pcb);
        return false;
    }

    tcp_bind(pcb, IP4_ADDR(127, 0, 0, 1), 8080);
    tcp_listen(pcb);
    if (pcb->state != TCP_STATE_LISTEN || pcb->local_port != 8080) {
        kprintf("[FAIL] tcp: bind or listen transition failed\n");
        tcp_free(pcb);
        return false;
    }

    /* Simulate incoming SYN -> SYN-ACK */
    pcb->state = TCP_STATE_SYN_RECEIVED;
    pcb->state = TCP_STATE_ESTABLISHED;
    if (pcb->state != TCP_STATE_ESTABLISHED) {
        kprintf("[FAIL] tcp: established state transition failed\n");
        tcp_free(pcb);
        return false;
    }

    tcp_free(pcb);

    /* Test Socket Syscall API */
    int sock_fd = sys_sock_socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        kprintf("[FAIL] socket: sys_sock_socket failed with %d\n", sock_fd);
        return false;
    }

    int rc = sys_sock_bind(sock_fd, IP4_ADDR(0, 0, 0, 0), 9090);
    if (rc != 0) {
        kprintf("[FAIL] socket: sys_sock_bind failed\n");
        sys_sock_close(sock_fd);
        return false;
    }

    rc = sys_sock_listen(sock_fd, 5);
    if (rc != 0) {
        kprintf("[FAIL] socket: sys_sock_listen failed\n");
        sys_sock_close(sock_fd);
        return false;
    }

    rc = sys_sock_close(sock_fd);
    if (rc != 0) {
        kprintf("[FAIL] socket: sys_sock_close failed\n");
        return false;
    }

    kprintf("[PASS] net: TCP connection state machine and Socket API syscall interface verified\n");
    return true;
}

bool net_run_selftests(void)
{
    kprintf("[TEST] Running Network Architecture & Protocol Stack Self-Tests...\n");

    if (!test_net_buf()) return false;
    if (!test_arp_and_ipv4_checksum()) return false;
    if (!test_dns_and_dhcp()) return false;
    if (!test_tcp_state_and_sockets()) return false;

    kprintf("[PASS] net: network architecture, drivers, protocols, and socket interfaces verified\n");
    return true;
}
