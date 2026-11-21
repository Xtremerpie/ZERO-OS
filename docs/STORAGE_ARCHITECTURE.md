# ZERO OS Storage Architecture & Block Subsystem

**Milestone:** 4.0.0 Storage & Networking  
**Subsystems:** Block Devices, Partition Tables (MBR/GPT), Block Cache (bcache), ZEROFS

---

## 1. Architectural Overview

The ZERO OS storage hierarchy bridges physical hardware drivers, virtual block devices, memory caching, and filesystem layers:

```
+-------------------------------------------------------------+
|                Virtual File System (VFS)                    |
|             / (RAMFS)      /data (ZEROFS)                   |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|              ZEROFS Native Filesystem Engine                |
|      Superblock | Bitmaps | Inode Table | Direct/Indirect   |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|             Block Buffer Cache (bcache, 4 KiB)              |
|        LRU Replacement | Dirty Tracking | Sync/Flush        |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|               Storage Manager & Partition Layer             |
|        MBR / GPT Parsers -> Partition Devices (devp1)       |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                 Block Device Abstraction                    |
|         VirtIO-Block | RAM Disk | IDE/ATA | NVMe            |
+-------------------------------------------------------------+
```

---

## 2. Partition Discovery Subsystem

### 2.1 MBR (Master Boot Record)
- Sector 0 holds bootstrap code, 4 16-byte partition records at offset 446 (`0x1BE`), and signature `0xAA55` at bytes 510-511.
- Validates boundary constraints:
  $$\text{start\_lba} + \text{sector\_count} \le \text{device\_sector\_count}$$
- Integer overflow guard: Rejects partitions where `start_lba + sector_count < start_lba`.
- Detects Protective GPT partition (type `0xEE`).

### 2.2 GPT (GUID Partition Table)
- Resides at LBA 1 with secondary backup at the final disk sector.
- Magic signature: `EFI PART` (`0x5452415020494645ULL`).
- Validation includes standard IEEE 802.3 CRC32 calculation over the header with the CRC field set to 0.
- Supports up to 128 partition entries (128 bytes each).
- Converts UTF-16LE partition names to standard ASCII strings.

### 2.3 Partition Devices
Discovered partitions are registered as child block devices:
- Operations delegate to parent block device with an LBA offset:
  $$\text{physical\_lba} = \text{partition\_start\_lba} + \text{relative\_lba}$$
- Enforces hard sector limits preventing partition breakouts.

---

## 3. Block Buffer Cache (`bcache`)

- **Block Size:** 4096 bytes (matches hardware virtual memory page size).
- **Entries:** 64 cached blocks (256 KiB cache footprint, ideal for low-memory hosts).
- **LRU Eviction:** Tracks timestamp of last access; evicts oldest unpinned block.
- **Write Policies:**
  * **Write-back:** Dirties block in memory, flushes during synchronization or eviction.
  * **Write-through:** Immediately writes to physical block device.
- **Synchronization:** `bcache_sync(dev)` flushes all dirty blocks belonging to a device, then calls device hardware cache flush.
