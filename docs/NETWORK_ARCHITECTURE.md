# ZERO OS Network Architecture & Protocol Stack

**Milestone:** 4.0.0 Storage & Networking  
**Subsystems:** Packet Buffers (`net_buf`), Interface Abstraction (`net_if`), Protocol Engines, Sockets

---

## 1. Network Stack Layering

```
+-------------------------------------------------------------+
|                  BSD Socket API & Syscalls                  |
|      sys_sock_socket | bind | listen | accept | send | recv  |
+-------------------------------------------------------------+
                              |
+------------------------------+------------------------------+
|     Transmission Control     |     User Datagram Protocol   |
|         Protocol (TCP)       |             (UDP)            |
|       RFC 793 State Machine  |     DHCP Client | DNS Client |
+------------------------------+------------------------------+
                              |
+-------------------------------------------------------------+
|             Internet Protocol Version 4 (IPv4)              |
|        RFC 791 Checksum | Routing | ICMP Echo Responder     |
+-------------------------------------------------------------+
                              |
+------------------------------+------------------------------+
|         Ethernet II          | Address Resolution Protocol  |
|      802.3 Frame Dispatch    |           (ARP)              |
+------------------------------+------------------------------+
                              |
+-------------------------------------------------------------+
|                  Network Drivers Layer                      |
|       Intel 82540EM (E1000) DMA Controller | Loopback       |
+-------------------------------------------------------------+
```

---

## 2. Packet Buffer Subsystem (`net_buf`)

- **Fixed Size:** 2048 bytes per buffer.
- **Headroom:** Pre-allocates 128 bytes at the front of the buffer.
- **Zero-Copy Encapsulation:**
  * When an upper layer hands data down, each protocol layer simply moves the `data` pointer backward (`net_buf_push`) without copying memory.
  * When parsing inbound packets, lower layers advance the `data` pointer forward (`net_buf_pull`).
- **Buffer Pool:** A static pool of 64 reference-counted buffers protected by a spinlock.

---

## 3. Protocol Implementations

### 3.1 Ethernet II & ARP
- **Ethernet II:** Handles frames with standard 14-byte headers. Dispatches EtherType `0x0806` to ARP and `0x0800` to IPv4.
- **ARP (RFC 826):**
  * Automatically resolves IPv4 addresses to 48-bit MAC addresses.
  * Maintains a 32-entry dynamic ARP cache with aging.
  * Automatically answers ARP requests targeting `netif->ip`.

### 3.2 IPv4 & ICMP
- **IPv4 (RFC 791):**
  * Computes and verifies 16-bit one's complement Internet checksums.
  * Routes packets destined for the local subnet directly, or through the default gateway for remote subnets.
- **ICMP (RFC 792):**
  * Immediate responder for Echo Requests (Type 8) -> Echo Replies (Type 0).
  * Outbound ping transmission support.

### 3.3 UDP, DHCP, and DNS
- **UDP (RFC 768):** Multiplexes datagrams based on 16-bit source and destination port numbers.
- **DHCP Client (RFC 2131):**
  * Executes state machine: `INIT` -> `DISCOVERING` -> `REQUESTING` -> `BOUND`.
  * Parses Subnet Mask (Option 1), Gateway (Option 3), and DNS Server (Option 6).
  * Automatically configures interface addressing upon receipt of `DHCP_ACK`.
- **DNS Resolver (RFC 1035):**
  * Formats standard DNS query packets using QNAME domain label encoding.
  * Parses answer records to resolve hostnames to IPv4 addresses.

### 3.4 Transmission Control Protocol (TCP)
- **RFC 793 State Machine:** `CLOSED`, `LISTEN`, `SYN_SENT`, `SYN_RECEIVED`, `ESTABLISHED`, `FIN_WAIT_1`, `FIN_WAIT_2`, `CLOSE_WAIT`, `CLOSING`, `LAST_ACK`, `TIME_WAIT`.
- Implements 3-way handshake with sequence and acknowledgement number verification.
- Sliding window receive buffer (4096 bytes).
- Full pseudo-header checksum calculation over IPv4 addresses and TCP payload.

---

## 4. Socket API & System Calls

The kernel exposes BSD-style socket descriptors mapped to numbers 11 through 17:
- `SYS_SOCKET` (11): Allocates a stream or datagram socket descriptor.
- `SYS_BIND` (12): Binds local IP and port.
- `SYS_LISTEN` (13): Sets TCP socket to listening mode with a connection backlog.
- `SYS_ACCEPT` (14): Accepts incoming established TCP connections.
- `SYS_CONNECT` (15): Initiates TCP 3-way handshake to remote host.
- `SYS_SEND` (16): Transmits data over stream or datagram socket.
- `SYS_RECV` (17): Reads received data from socket buffer queue.
