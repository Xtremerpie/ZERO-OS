/* ZERO KERNEL - Kernel Socket Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements socket descriptors, protocol mapping, and system call handlers.
 */
#include "net/socket.h"
#include "net/udp.h"
#include "net/ipv4.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "zero/string.h"

static zero_socket_t g_sockets[SOCKET_MAX_DESCRIPTORS];
static spinlock_t    g_sock_lock;

void socket_subsystem_init(void)
{
    spinlock_init(&g_sock_lock);
    spin_lock(&g_sock_lock);
    for (size_t i = 0; i < SOCKET_MAX_DESCRIPTORS; i++) {
        g_sockets[i].id = (int)i;
        g_sockets[i].in_use = false;
        g_sockets[i].tcp_pcb = NULL;
    }
    spin_unlock(&g_sock_lock);
    KLOG_INFO("socket", "initialized kernel socket table (%u slots)", SOCKET_MAX_DESCRIPTORS);
}

zero_socket_t *socket_get(int fd)
{
    if (fd < 0 || (size_t)fd >= SOCKET_MAX_DESCRIPTORS) return NULL;
    return &g_sockets[fd];
}

int sys_sock_socket(int domain, int type, int protocol)
{
    if (domain != AF_INET) return -1;

    spin_lock(&g_sock_lock);
    for (size_t i = 0; i < SOCKET_MAX_DESCRIPTORS; i++) {
        if (!g_sockets[i].in_use) {
            zero_socket_t *s = &g_sockets[i];
            memset(s, 0, sizeof(*s));
            s->id = (int)i;
            s->in_use = true;
            s->domain = domain;
            s->type = type;
            s->protocol = protocol;

            if (type == SOCK_STREAM) {
                s->tcp_pcb = tcp_alloc();
                if (!s->tcp_pcb) {
                    s->in_use = false;
                    spin_unlock(&g_sock_lock);
                    return -2;
                }
            }

            spin_unlock(&g_sock_lock);
            return (int)i;
        }
    }
    spin_unlock(&g_sock_lock);
    return -3; /* Out of socket descriptors */
}

int sys_sock_bind(int fd, uint32_t ip, uint16_t port)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use) return -1;

    s->local_ip = ip;
    s->local_port = port;

    if (s->type == SOCK_STREAM && s->tcp_pcb) {
        return tcp_bind(s->tcp_pcb, ip, port);
    }
    return 0;
}

int sys_sock_listen(int fd, int backlog)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use || s->type != SOCK_STREAM) return -1;

    s->is_listening = true;
    s->backlog = backlog;
    if (s->tcp_pcb) {
        return tcp_listen(s->tcp_pcb);
    }
    return 0;
}

int sys_sock_accept(int fd, uint32_t *out_remote_ip, uint16_t *out_remote_port)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use || !s->is_listening || !s->tcp_pcb) return -1;

    if (s->tcp_pcb->state == TCP_STATE_ESTABLISHED) {
        int client_fd = sys_sock_socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd < 0) return -2;

        zero_socket_t *cs = socket_get(client_fd);
        cs->remote_ip = s->tcp_pcb->remote_ip;
        cs->remote_port = s->tcp_pcb->remote_port;
        cs->local_ip = s->tcp_pcb->local_ip;
        cs->local_port = s->tcp_pcb->local_port;

        /* Move active PCB to child socket */
        cs->tcp_pcb = s->tcp_pcb;

        /* Reset listener with fresh PCB */
        s->tcp_pcb = tcp_alloc();
        if (s->tcp_pcb) {
            tcp_bind(s->tcp_pcb, s->local_ip, s->local_port);
            tcp_listen(s->tcp_pcb);
        }

        if (out_remote_ip) *out_remote_ip = cs->remote_ip;
        if (out_remote_port) *out_remote_port = cs->remote_port;

        return client_fd;
    }

    return -3; /* No connection ready */
}

int sys_sock_connect(int fd, uint32_t ip, uint16_t port)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use) return -1;

    s->remote_ip = ip;
    s->remote_port = port;

    if (s->type == SOCK_STREAM && s->tcp_pcb) {
        return tcp_connect(s->tcp_pcb, ip, port);
    }
    return 0;
}

int64_t sys_sock_send(int fd, const void *buf, size_t len)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use || !buf) return -1;

    if (s->type == SOCK_STREAM && s->tcp_pcb) {
        int rc = tcp_send(s->tcp_pcb, buf, len);
        return (rc == 0) ? (int64_t)len : -2;
    } else if (s->type == SOCK_DGRAM) {
        net_if_t *netif = net_if_get_default();
        if (!netif) return -3;

        net_buf_t *nbuf = net_buf_alloc(NET_BUF_HEADROOM + sizeof(udp_hdr_t) + sizeof(ipv4_hdr_t));
        if (!nbuf) return -4;

        void *p = net_buf_put(nbuf, len);
        if (!p) {
            net_buf_free(nbuf);
            return -5;
        }
        memcpy(p, buf, len);

        int rc = udp_tx(netif, nbuf, s->local_port, s->remote_ip, s->remote_port);
        return (rc == 0) ? (int64_t)len : -6;
    }

    return -7;
}

int64_t sys_sock_recv(int fd, void *buf, size_t len)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use || !buf || len == 0) return 0;

    if (s->type == SOCK_STREAM && s->tcp_pcb) {
        return (int64_t)tcp_recv(s->tcp_pcb, buf, len);
    } else if (s->type == SOCK_DGRAM) {
        spin_lock(&g_sock_lock);
        size_t to_read = s->dgram_len;
        if (to_read > len) to_read = len;
        if (to_read > 0) {
            memcpy(buf, s->dgram_buf, to_read);
            s->dgram_len = 0;
        }
        spin_unlock(&g_sock_lock);
        return (int64_t)to_read;
    }

    return 0;
}

int sys_sock_close(int fd)
{
    zero_socket_t *s = socket_get(fd);
    if (!s || !s->in_use) return -1;

    spin_lock(&g_sock_lock);
    if (s->type == SOCK_STREAM && s->tcp_pcb) {
        tcp_close(s->tcp_pcb);
        tcp_free(s->tcp_pcb);
        s->tcp_pcb = NULL;
    }
    s->in_use = false;
    spin_unlock(&g_sock_lock);

    return 0;
}
