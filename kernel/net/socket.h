/* ZERO KERNEL - Kernel Socket Abstraction & API (Milestone 4.0.0 Storage & Networking).
 *
 * Implements BSD-compatible socket descriptors for AF_INET (SOCK_STREAM TCP,
 * SOCK_DGRAM UDP, and SOCK_RAW), mapped into VFS and system call interfaces.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "net/tcp.h"

#define AF_INET         2
#define SOCK_STREAM     1 /* TCP */
#define SOCK_DGRAM      2 /* UDP */
#define SOCK_RAW        3 /* ICMP/Raw IP */

#define SOCKET_MAX_DESCRIPTORS 32u

typedef struct zero_socket {
    int         id;
    int         domain;
    int         type;
    int         protocol;
    uint32_t    local_ip;
    uint16_t    local_port;
    uint32_t    remote_ip;
    uint16_t    remote_port;

    tcp_pcb_t  *tcp_pcb;
    bool        is_listening;
    int         backlog;

    /* DGRAM receive buffer */
    uint8_t     dgram_buf[TCP_BUFFER_SIZE];
    size_t      dgram_len;

    bool        in_use;
} zero_socket_t;

/* Initialize socket subsystem */
void socket_subsystem_init(void);

/* Socket API functions matching standard syscall contracts */
int sys_sock_socket(int domain, int type, int protocol);
int sys_sock_bind(int fd, uint32_t ip, uint16_t port);
int sys_sock_listen(int fd, int backlog);
int sys_sock_accept(int fd, uint32_t *out_remote_ip, uint16_t *out_remote_port);
int sys_sock_connect(int fd, uint32_t ip, uint16_t port);
int64_t sys_sock_send(int fd, const void *buf, size_t len);
int64_t sys_sock_recv(int fd, void *buf, size_t len);
int sys_sock_close(int fd);

/* Retrieve socket descriptor pointer */
zero_socket_t *socket_get(int fd);
