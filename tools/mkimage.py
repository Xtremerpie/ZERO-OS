#!/usr/bin/env python3
"""ZERO OS - UEFI Bootable Disk Image Generator (Pure Python)

Constructs a 64 MiB GPT disk image with a FAT32/FAT16 EFI System Partition (ESP)
containing Limine UEFI bootloader (BOOTX64.EFI), limine.conf, and zero-kernel.elf.
Runs on Windows or Linux with zero external tool dependencies (no sgdisk or mtools needed).
"""
import os
import sys
import struct
import zlib
import uuid
import hashlib

IMAGE_SIZE_BYTES = 64 * 1024 * 1024  # 64 MiB
SECTOR_SIZE = 512
TOTAL_SECTORS = IMAGE_SIZE_BYTES // SECTOR_SIZE  # 131,072 sectors
PART_START_LBA = 2048  # 1 MiB offset
PART_END_LBA = TOTAL_SECTORS - 34  # Reserve 33 sectors for backup GPT + 1
PART_SECTORS = PART_END_LBA - PART_START_LBA + 1

# GUIDs (RFC 4122 binary representation)
# EFI System Partition: C12A7328-F81F-11D2-BA4B-00A0C93EC93B
ESP_TYPE_GUID = bytes([
    0x28, 0x73, 0x2A, 0xC1,  # Data1 (little-endian)
    0x1F, 0xF8,              # Data2 (little-endian)
    0xD2, 0x11,              # Data3 (little-endian)
    0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B  # Data4 (big-endian)
])

def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

def build_protective_mbr() -> bytes:
    mbr = bytearray(512)
    # Partition entry 1 at offset 446 (0x1BE)
    mbr[446] = 0x00       # Bootable flag (0 = non-bootable)
    mbr[447] = 0x00       # Starting head
    mbr[448] = 0x02       # Starting sector
    mbr[449] = 0x00       # Starting cylinder
    mbr[450] = 0xEE       # Partition type: 0xEE (GPT Protective)
    mbr[451] = 0xFF       # Ending head
    mbr[452] = 0xFF       # Ending sector
    mbr[453] = 0xFF       # Ending cylinder
    struct.pack_into("<I", mbr, 454, 1)  # Starting LBA = 1
    struct.pack_into("<I", mbr, 458, min(TOTAL_SECTORS - 1, 0xFFFFFFFF))  # Size in sectors
    mbr[510] = 0x55
    mbr[511] = 0xAA
    return bytes(mbr)

def build_gpt_entry(part_type_guid: bytes, unique_guid: bytes, start_lba: int, end_lba: int, name: str) -> bytes:
    entry = bytearray(128)
    entry[0:16] = part_type_guid
    entry[16:32] = unique_guid
    struct.pack_into("<Q", entry, 32, start_lba)
    struct.pack_into("<Q", entry, 40, end_lba)
    struct.pack_into("<Q", entry, 48, 0)  # Attributes
    name_bytes = name.encode("utf-16le")[:72]
    entry[56:56+len(name_bytes)] = name_bytes
    return bytes(entry)

def build_gpt_header(current_lba: int, backup_lba: int, first_usable: int, last_usable: int,
                     disk_guid: bytes, part_entry_lba: int, entries_crc: int) -> bytes:
    hdr = bytearray(92)
    hdr[0:8] = b"EFI PART"
    struct.pack_into("<I", hdr, 8, 0x00010000)  # Revision 1.0
    struct.pack_into("<I", hdr, 12, 92)         # Header size
    struct.pack_into("<I", hdr, 16, 0)          # CRC32 placeholder
    struct.pack_into("<I", hdr, 20, 0)          # Reserved
    struct.pack_into("<Q", hdr, 24, current_lba)
    struct.pack_into("<Q", hdr, 32, backup_lba)
    struct.pack_into("<Q", hdr, 40, first_usable)
    struct.pack_into("<Q", hdr, 48, last_usable)
    hdr[56:72] = disk_guid
    struct.pack_into("<Q", hdr, 72, part_entry_lba)
    struct.pack_into("<I", hdr, 80, 128)        # Number of partition entries
    struct.pack_into("<I", hdr, 84, 128)        # Size of each entry (128 bytes)
    struct.pack_into("<I", hdr, 88, entries_crc)
    # Calculate CRC32 of header
    c = crc32(hdr)
    struct.pack_into("<I", hdr, 16, c)
    # Pad to 512 bytes
    return bytes(hdr) + b"\x00" * (512 - 92)

class FAT32Builder:
    def __init__(self, total_sectors: int):
        self.total_sectors = total_sectors
        self.sectors_per_cluster = 1  # 512 bytes/cluster -> ~128k clusters (valid FAT32)
        self.reserved_sectors = 32
        self.num_fats = 2
        
        # Calculate FAT size: each cluster has 4 bytes in FAT.
        # clusters ~ (total_sectors - reserved) / sectors_per_cluster
        # sectors_per_fat ~ (clusters * 4 + 511) // 512
        num_clusters = total_sectors - self.reserved_sectors
        self.sectors_per_fat = (num_clusters * 4 + 511) // 512
        
        self.fat_start_sector = self.reserved_sectors
        self.data_start_sector = self.reserved_sectors + self.num_fats * self.sectors_per_fat
        self.total_data_clusters = (total_sectors - self.data_start_sector) // self.sectors_per_cluster
        
        # Allocate FAT tables
        self.fat_entries = [0] * (self.total_data_clusters + 2)
        # Cluster 0 and 1 are reserved
        self.fat_entries[0] = 0x0FFFFFF8
        self.fat_entries[1] = 0x0FFFFFFF
        # Cluster 2 is root directory
        self.fat_entries[2] = 0x0FFFFFFF
        self.next_free_cluster = 3
        
        # In-memory storage for clusters (cluster_num -> 512 bytes)
        self.clusters = {}
        self.clusters[2] = bytearray(512)  # Root dir
        
    def alloc_cluster(self) -> int:
        c = self.next_free_cluster
        self.next_free_cluster += 1
        self.fat_entries[c] = 0x0FFFFFFF
        self.clusters[c] = bytearray(512)
        return c

    def write_file(self, data: bytes) -> (int, int):
        """Writes data into a cluster chain. Returns (first_cluster, file_size)."""
        if len(data) == 0:
            return 0, 0
        chunks = [data[i:i+512] for i in range(0, len(data), 512)]
        first_cluster = self.alloc_cluster()
        cur_cluster = first_cluster
        self.clusters[cur_cluster][:len(chunks[0])] = chunks[0]
        
        for chunk in chunks[1:]:
            next_cluster = self.alloc_cluster()
            self.fat_entries[cur_cluster] = next_cluster
            self.clusters[next_cluster][:len(chunk)] = chunk
            cur_cluster = next_cluster
        return first_cluster, len(data)

    def add_dirent(self, dir_cluster: int, name83: bytes, attr: int, first_cluster: int, size: int):
        d = self.clusters[dir_cluster]
        # Find free 32-byte slot
        for offset in range(0, len(d), 32):
            if d[offset] == 0x00 or d[offset] == 0xE5:
                # Format 32-byte directory entry
                d[offset:offset+11] = name83
                d[offset+11] = attr
                # Time/date placeholders
                struct.pack_into("<H", d, offset+20, (first_cluster >> 16) & 0xFFFF)
                struct.pack_into("<H", d, offset+26, first_cluster & 0xFFFF)
                struct.pack_into("<I", d, offset+28, size)
                return
        # If full, extend directory by one cluster
        new_c = self.alloc_cluster()
        self.fat_entries[dir_cluster] = new_c
        # Add to new cluster
        d = self.clusters[new_c]
        d[0:11] = name83
        d[11] = attr
        struct.pack_into("<H", d, 20, (first_cluster >> 16) & 0xFFFF)
        struct.pack_into("<H", d, 26, first_cluster & 0xFFFF)
        struct.pack_into("<I", d, 28, size)

    def make_dir(self, parent_cluster: int, name83: bytes) -> int:
        dir_c = self.alloc_cluster()
        # Add entry in parent
        self.add_dirent(parent_cluster, name83, 0x10, dir_c, 0)
        # Add . and .. in new dir
        d = self.clusters[dir_c]
        # "."
        d[0:11] = b".          "
        d[11] = 0x10
        struct.pack_into("<H", d, 20, (dir_c >> 16) & 0xFFFF)
        struct.pack_into("<H", d, 26, dir_c & 0xFFFF)
        # ".."
        d[32:43] = b"..         "
        d[43] = 0x10
        p_c = 0 if parent_cluster == 2 else parent_cluster
        struct.pack_into("<H", d, 52, (p_c >> 16) & 0xFFFF)
        struct.pack_into("<H", d, 58, p_c & 0xFFFF)
        return dir_c

    def to_bytes(self) -> bytes:
        vbr = bytearray(512)
        vbr[0:3] = b"\xEB\x58\x90"
        vbr[3:11] = b"ZEROESP "
        struct.pack_into("<H", vbr, 11, 512)                     # Bytes per sector
        vbr[13] = self.sectors_per_cluster                      # Sectors per cluster
        struct.pack_into("<H", vbr, 14, self.reserved_sectors)   # Reserved sectors
        vbr[16] = self.num_fats                                  # Number of FATs
        struct.pack_into("<H", vbr, 17, 0)                       # Root entry count (FAT12/16 only)
        struct.pack_into("<H", vbr, 19, 0)                       # Total sectors 16-bit
        vbr[21] = 0xF8                                           # Media descriptor
        struct.pack_into("<H", vbr, 22, 0)                       # Sectors per FAT (FAT12/16)
        struct.pack_into("<H", vbr, 24, 63)                      # Sectors per track
        struct.pack_into("<H", vbr, 26, 255)                     # Number of heads
        struct.pack_into("<I", vbr, 28, PART_START_LBA)          # Hidden sectors
        struct.pack_into("<I", vbr, 32, self.total_sectors)      # Total sectors 32-bit
        # FAT32 extended BPB
        struct.pack_into("<I", vbr, 36, self.sectors_per_fat)    # Sectors per FAT 32-bit
        struct.pack_into("<H", vbr, 40, 0)                       # Flags
        struct.pack_into("<H", vbr, 42, 0)                       # Version 0.0
        struct.pack_into("<I", vbr, 44, 2)                       # Root dir cluster = 2
        struct.pack_into("<H", vbr, 48, 1)                       # FSInfo sector = 1
        struct.pack_into("<H", vbr, 50, 6)                       # Backup boot sector = 6
        vbr[64] = 0x80                                           # Drive number
        vbr[66] = 0x29                                           # Extended boot signature
        struct.pack_into("<I", vbr, 67, 0x12345678)              # Volume ID
        vbr[71:82] = b"ZEROESP    "                              # Volume label
        vbr[82:90] = b"FAT32   "                                 # FS type
        vbr[510] = 0x55
        vbr[511] = 0xAA

        # FSInfo sector
        fsinfo = bytearray(512)
        fsinfo[0:4] = b"RRaA"
        fsinfo[484:488] = b"rrAa"
        struct.pack_into("<I", fsinfo, 488, 0xFFFFFFFF)  # Free cluster count
        struct.pack_into("<I", fsinfo, 492, 0xFFFFFFFF)  # Next free cluster
        fsinfo[510] = 0x55
        fsinfo[511] = 0xAA

        # Build raw partition buffer
        buf = bytearray(self.total_sectors * 512)
        buf[0:512] = vbr
        buf[512:1024] = fsinfo

        # Build FAT bytes
        fat_bytes = bytearray(self.sectors_per_fat * 512)
        for i, val in enumerate(self.fat_entries):
            struct.pack_into("<I", fat_bytes, i * 4, val & 0x0FFFFFFF)

        # Copy FAT1 and FAT2
        fat1_offset = self.fat_start_sector * 512
        fat2_offset = fat1_offset + self.sectors_per_fat * 512
        buf[fat1_offset : fat1_offset + len(fat_bytes)] = fat_bytes
        buf[fat2_offset : fat2_offset + len(fat_bytes)] = fat_bytes

        # Copy clusters
        data_offset = self.data_start_sector * 512
        for c_idx, c_data in self.clusters.items():
            c_offset = data_offset + (c_idx - 2) * self.sectors_per_cluster * 512
            buf[c_offset : c_offset + len(c_data)] = c_data

        return bytes(buf)

def generate_uefi_disk_image(kernel_path: str, out_path: str, cmdline: str = "") -> str:
    print(f"Building UEFI GPT Disk Image: {out_path}")
    print(f"  Kernel: {kernel_path}")
    
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    limine_efi = os.path.join(root_dir, "third_party", "limine", "BOOTX64.EFI")
    if not os.path.exists(limine_efi):
        raise FileNotFoundError(f"Missing {limine_efi}")

    with open(limine_efi, "rb") as f:
        bootx64_data = f.read()

    conf_content = f"""# ZERO OS - Limine configuration
timeout: 0
serial: yes
quiet: yes

/ZERO OS
    protocol: limine
    path: boot():/boot/zero-kernel.elf
    cmdline: {cmdline}
""".replace("\r\n", "\n").encode("utf-8")

    if os.path.exists(kernel_path):
        with open(kernel_path, "rb") as f:
            kernel_data = f.read()
    else:
        print(f"  [WARN] Kernel ELF not present at {kernel_path} (generating image with bootloader & config)")
        kernel_data = b""

    # 1. Build FAT32 Volume
    fat32 = FAT32Builder(PART_SECTORS)
    
    # Create directories: /EFI, /EFI/BOOT, /boot
    efi_dir = fat32.make_dir(2, b"EFI        ")
    boot_dir_efi = fat32.make_dir(efi_dir, b"BOOT       ")
    boot_dir_root = fat32.make_dir(2, b"BOOT       ")

    # Write files:
    # 1. /EFI/BOOT/BOOTX64.EFI
    c_boot, s_boot = fat32.write_file(bootx64_data)
    fat32.add_dirent(boot_dir_efi, b"BOOTX64 EFI", 0x20, c_boot, s_boot)

    # 2. /limine.conf
    c_conf, s_conf = fat32.write_file(conf_content)
    fat32.add_dirent(2, b"LIMINECONF ", 0x20, c_conf, s_conf)

    # 3. /boot/zero-kernel.elf (if kernel present)
    if kernel_data:
        c_k, s_k = fat32.write_file(kernel_data)
        fat32.add_dirent(boot_dir_root, b"ZERO-KE ELF", 0x20, c_k, s_k)

    partition_bytes = fat32.to_bytes()

    # 2. Build Disk Structure
    disk = bytearray(IMAGE_SIZE_BYTES)
    
    # LBA 0: MBR
    disk[0:512] = build_protective_mbr()

    # LBA 2-33: Partition Table Entries
    disk_guid = uuid.uuid4().bytes
    part_guid = uuid.uuid4().bytes
    entries = bytearray(128 * 128)
    entries[0:128] = build_gpt_entry(ESP_TYPE_GUID, part_guid, PART_START_LBA, PART_END_LBA, "ZERO-ESP")
    entries_crc = crc32(entries)
    disk[2 * 512 : (2 + 32) * 512] = entries

    # LBA 1: Primary GPT Header
    primary_hdr = build_gpt_header(1, TOTAL_SECTORS - 1, 34, TOTAL_SECTORS - 34,
                                   disk_guid, 2, entries_crc)
    disk[512:1024] = primary_hdr

    # LBA 2048: Partition Data
    part_offset = PART_START_LBA * 512
    disk[part_offset : part_offset + len(partition_bytes)] = partition_bytes

    # Backup GPT Entries (last 33 sectors: total_sectors - 33 to total_sectors - 2)
    backup_entries_lba = TOTAL_SECTORS - 33
    disk[backup_entries_lba * 512 : (backup_entries_lba + 32) * 512] = entries

    # Backup GPT Header (last sector: total_sectors - 1)
    backup_hdr = build_gpt_header(TOTAL_SECTORS - 1, 1, 34, TOTAL_SECTORS - 34,
                                  disk_guid, backup_entries_lba, entries_crc)
    disk[(TOTAL_SECTORS - 1) * 512 : TOTAL_SECTORS * 512] = backup_hdr

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(disk)

    h = hashlib.sha256(disk).hexdigest()
    print(f"  [OK] Generated {out_path} ({len(disk):,} bytes)")
    print(f"  [OK] SHA-256: {h}")
    return out_path

if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    kernel = os.path.join(root, "build", "zero-kernel.elf")
    out = os.path.join(root, "build", "zero-os.img")
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    generate_uefi_disk_image(kernel, out, cmd)
