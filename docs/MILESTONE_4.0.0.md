# ZERO OS 4.0.0 — Storage & Networking Milestone Report

**Codename:** Storage & Networking  
**Architecture:** x86_64 Higher-Half Kernel  
**Boot Protocol:** UEFI + Limine Bootloader  
**Status:** COMPLETE (Milestone 4.0.0)

---

## 1. Executive Summary

ZERO OS 4.0.0 achieves a milestone in custom operating system engineering by delivering a native storage subsystem, a resilient on-disk filesystem (**ZEROFS**), an LRU block buffer cache (**bcache**), MBR and GPT partition table parsers, a full RFC-compliant network protocol stack (**Ethernet II, ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP**), an Intel 82540EM (E1000) Gigabit Ethernet controller driver, and a BSD-compatible Socket API integrated into the kernel's fast system call interface.

ZERO OS remains entirely independent: built from scratch with zero Linux, Windows, or third-party userspace dependencies.

---

## 2. Key Subsystems Delivered

### 2.1 Central Storage Architecture (`kernel/storage/storage.c`, `storage.h`)
- Unified block device manager wrapping physical drivers (VirtIO-Block, IDE/ATA, NVMe) and virtual block devices (RAM disks, partition slices).
- Strict bounds checking and 64-bit integer overflow protection on all sector read/write operations.
- Global storage traffic metrics: reads completed, writes completed, sectors transferred, and I/O error logging.

### 2.2 Partition Table Discovery (`kernel/storage/partition.c`, `partition.h`)
- **Master Boot Record (MBR):** Sector 0 signature validation (`0xAA55`), 4 primary partition entry decoders, bootable flag checking, CHS/LBA translation, and protective GPT detection (`0xEE`).
- **GUID Partition Table (GPT):** Primary header at LBA 1, signature verification (`EFI PART`), standard IEEE 802.3 CRC32 header and partition array verification, 128-byte partition entry decoding, UTF-16LE partition label conversion.
- **Partition Devices:** Automated registration of virtual `block_device_t` instances for discovered partitions (`devp1`, `devp2`), allowing filesystems to mount partition boundaries transparently.

### 2.3 Block Buffer Cache (`kernel/storage/bcache.c`, `bcache.h`)
- 64-entry LRU block cache buffering 4096-byte filesystem blocks.
- Write-back and write-through cache policies with per-block dirty tracking.
- Cache statistics: hit counters, miss counters, evictions, and synchronous write-back flushes.
- `bcache_sync()` and `bcache_flush_all()` routines guaranteeing crash resilience and on-disk consistency.

### 2.4 ZEROFS — Native Persistent Filesystem (`kernel/fs/zerofs.c`, `zerofs.h`)
- On-disk layout:
  * **Superblock (Block 1):** Magic `0x5A45524F` ("ZERO"), 4096-byte blocks, total/free block counters, total/free inode counters, volume name.
  * **Bitmaps:** Inode allocation bitmap (Block 2) and Block allocation bitmap (Block 3).
  * **Inode Table (Blocks 4..19):** 512 128-byte inodes containing file size, permission mode, timestamp fields, 12 direct block pointers (up to 48 KiB direct), and single indirect pointer (up to 4 MiB).
  * **Directory Entries (64 bytes):** Inode pointer, record length, filename (up to 56 characters), and file type.
- Full Virtual File System (VFS) integration: `zerofs_format()`, `zerofs_mount()`, path resolution, file create, multi-block read/write, directory creation, and unlinking.

### 2.5 Network Architecture & Buffer Management (`kernel/net/net_buf.c`, `net_buf.h`, `net.c`, `net.h`)
- Reference-counted `net_buf_t` packet buffers with 128-byte headroom, enabling zero-copy header encapsulation (`net_buf_push`) and strip (`net_buf_pull`).
- Unified network interface structure (`net_if_t`) tracking MAC, IPv4 address, subnet mask, gateway, DNS, MTU, packet counters, and byte traffic.

### 2.6 Intel 82540EM (E1000) Driver (`kernel/drivers/net/e1000.c`, `e1000.h`)
- PCI bus probing (`0x8086:0x100E`, `0x1004`, `0x100F`).
- MMIO register mapping via Higher-Half Direct Map (HHDM).
- 32-descriptor DMA Transmit (TX) and Receive (RX) circular rings with hardware tail pointer advancement.
- Hardware link detection and loopback fallback for deterministic environments.

### 2.7 Network Protocol Stack (`kernel/net/`)
- **Ethernet II (`ethernet.c`):** Standard 802.3 framing, EtherType dispatch (`0x0800` IPv4, `0x0806` ARP), broadcast address filtering.
- **ARP (`arp.c`):** RFC 826 request generation, reply synthesis, and 32-entry dynamic ARP cache.
- **IPv4 (`ipv4.c`):** RFC 791 header parsing, standard 16-bit one's complement Internet checksum calculation and verification, direct subnet vs default gateway route selection.
- **ICMP (`icmp.c`):** RFC 792 Echo Request (Type 8) and Echo Reply (Type 0) responder.
- **UDP (`udp.c`):** RFC 768 datagram framing and port-based demultiplexing.
- **DHCP (`dhcp.c`):** RFC 2131 client state machine (INIT -> DISCOVERING -> REQUESTING -> BOUND), parsing Subnet Mask (Option 1), Gateway (Option 3), and DNS (Option 6).
- **DNS (`dns.c`):** RFC 1035 query building, QNAME label encoding, and A-record address parsing.
- **TCP (`tcp.c`):** RFC 793 connection state machine, 3-way handshake (SYN -> SYN-ACK -> ACK), sequence/acknowledgement tracking, sliding window buffers, and FIN teardown.

### 2.8 Kernel Socket API & Syscalls (`kernel/net/socket.c`, `socket.h`, `kernel/syscall/syscall.c`)
- BSD-compatible socket descriptors supporting `AF_INET` with `SOCK_STREAM` (TCP), `SOCK_DGRAM` (UDP), and `SOCK_RAW`.
- Fast system call bindings:
  * `SYS_SOCKET` (11), `SYS_BIND` (12), `SYS_LISTEN` (13), `SYS_ACCEPT` (14), `SYS_CONNECT` (15), `SYS_SEND` (16), `SYS_RECV` (17).

---

## 3. Test & Verification Summary

| Test Suite | Components Tested | Status |
| :--- | :--- | :--- |
| `selftest_storage.c` | MBR parsing, GPT CRC32, bcache read/write/flush | **PASS (100%)** |
| `selftest_zerofs.c` | Formatting, VFS mounting, multi-block I/O, mkdir, unlink | **PASS (100%)** |
| `selftest_net.c` | net_buf, Ethernet, ARP, IPv4 checksum, ICMP, DHCP, DNS, TCP, sockets | **PASS (100%)** |
| `test_storage_net.py` | 13 binary wire format and math validation test suites | **PASS (100%)** |
| `test_hardware.py` | ACPI, PCI, Input, Block, USB hardware test suites | **PASS (100%)** |
| `check_versions.py` | Exact-10 milestone manifest verification (4.0.0 active) | **PASS (100%)** |
