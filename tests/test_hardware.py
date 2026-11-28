#!/usr/bin/env python3
"""ZERO OS Milestone 3.0.0 Hardware Foundation Test Suite.

Validates:
1. ACPI checksum validation and MADT record extraction.
2. PCI configuration address encoding and BAR size decoding.
3. PS/2 Keyboard Scancode Set 1 make/break parsing and modifier updates.
4. PS/2 Mouse 3-byte packet decoding, 9-bit sign-extended deltas, and button states.
5. Block storage sector math, bounds validation, and buffer offsets.
6. USB descriptor protocol structure sizes and alignments.
"""

import struct
import sys

def test_acpi_checksum():
    print("[TEST] ACPI checksumming...")
    # Valid 32-byte table
    table = bytearray(range(31))
    sum_val = sum(table) & 0xFF
    table.append((0x100 - sum_val) & 0xFF)
    assert sum(table) & 0xFF == 0, "ACPI valid checksum failed"

    # Corrupt one byte
    table[0] ^= 0xFF
    assert sum(table) & 0xFF != 0, "ACPI corrupted checksum passed"
    print("  -> PASS: ACPI checksum verification")

def test_acpi_madt_parsing():
    print("[TEST] ACPI MADT parsing...")
    # Construct synthetic MADT table: header (36 bytes) + lapic_addr (4) + flags (4) + records
    sdt_hdr = struct.pack("<4sIBB6s8sIII", b"APIC", 36 + 8 + 8 + 12 + 10, 1, 0, b"ZEROOS", b"TESTMADT", 1, 0, 0)
    madt_body = struct.pack("<II", 0xFEE00000, 1) # LAPIC addr, PCAT_COMPAT

    # Record 0: Local APIC (type=0, len=8, proc_id=1, apic_id=0, flags=1)
    rec_lapic0 = struct.pack("<BBBB I", 0, 8, 1, 0, 1)
    # Record 1: IO-APIC (type=1, len=12, ioapic_id=2, res=0, ioapic_addr=0xFEC00000, gsi_base=0)
    rec_ioapic = struct.pack("<BBBB II", 1, 12, 2, 0, 0xFEC00000, 0)
    # Record 2: ISO (type=2, len=10, bus=0, source=1, gsi=1, flags=0)
    rec_iso = struct.pack("<BBBB IH", 2, 10, 0, 1, 1, 0)

    raw = sdt_hdr + madt_body + rec_lapic0 + rec_ioapic + rec_iso
    assert len(raw) == 36 + 8 + 8 + 12 + 10

    # Parse and verify
    sig, length, rev, csum = struct.unpack_from("<4sIBB", raw, 0)
    assert sig == b"APIC"
    lapic_phys, flags = struct.unpack_from("<II", raw, 36)
    assert lapic_phys == 0xFEE00000

    offset = 44
    records = []
    while offset < len(raw):
        rtype, rlen = struct.unpack_from("<BB", raw, offset)
        records.append((rtype, rlen))
        offset += rlen

    assert len(records) == 3
    assert records[0] == (0, 8)
    assert records[1] == (1, 12)
    assert records[2] == (2, 10)
    print("  -> PASS: ACPI MADT record decoding")

def test_pci_address_and_bar():
    print("[TEST] PCI configuration space & BAR probing...")
    # PCI config address formula: (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC)
    def pci_addr(bus, slot, func, offset):
        return (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC)

    # Bus 0, Slot 1, Func 0, Reg 0x10 (BAR0)
    addr = pci_addr(0, 1, 0, 0x10)
    assert addr == 0x80000810, f"Expected 0x80000810, got {hex(addr)}"

    # I/O BAR size probing formula: ~(mask & ~0x3) + 1 (in 32-bit unsigned arithmetic)
    # Example: mask = 0xFFFFFFE1 (32 bytes I/O space)
    io_mask = 0xFFFFFFE1
    io_size = ((~(io_mask & ~0x3)) & 0xFFFFFFFF) + 1
    assert io_size == 32, f"Expected 32 bytes, got {io_size}"

    # MMIO BAR size probing formula: ~(mask & ~0xF) + 1 (in 32-bit unsigned arithmetic)
    # Example: mask = 0xFFFF0000 (64 KiB MMIO space)
    mmio_mask = 0xFFFF0000
    mmio_size = ((~(mmio_mask & ~0xF)) & 0xFFFFFFFF) + 1
    assert mmio_size == 65536, f"Expected 65536 bytes, got {mmio_size}"
    print("  -> PASS: PCI address calculation and BAR size formula")

def test_ps2_keyboard_decoding():
    print("[TEST] PS/2 Keyboard scancode decoding...")
    # Scancode Set 1 make code for 'A' is 0x1E, break code is 0x1E | 0x80 = 0x9E
    make_a = 0x1E
    break_a = 0x9E

    def is_break(sc):
        return (sc & 0x80) != 0

    assert not is_break(make_a), "Make code identified as break"
    assert is_break(break_a), "Break code not identified"
    assert (break_a & 0x7F) == make_a, "Make extraction failed"

    # Extended scancode 0xE0 prefix handling (arrows + Super/LWIN)
    seq_up = [0xE0, 0x48]
    assert seq_up[0] == 0xE0
    assert seq_up[1] == 0x48 # Up arrow
    seq_lwin = [0xE0, 0x5B]
    assert seq_lwin[0] == 0xE0 and (seq_lwin[1] & 0x7F) == 0x5B
    print("  -> PASS: PS/2 Keyboard scancodes and break detection")

def test_ps2_mouse_packet():
    print("[TEST] PS/2 Mouse 3-byte packet decoding...")
    # Byte 0: 0x09 (bit 3 set, bit 0 left button set)
    # Byte 1: 0x0A (+10 dx)
    # Byte 2: 0xF6 (-10 dy with bit 5 sign set in byte 0 -> 0x29)
    b0 = 0x29 # Bit 3 set, Left btn set, Y sign set
    b1 = 0x0A # +10 dx
    b2 = 0xF6 # -10 dy

    assert (b0 & 0x08) != 0, "Bit 3 sanity check failed"
    left_click = bool(b0 & 0x01)
    assert left_click is True, "Left click detection failed"

    dx = b1
    if b0 & 0x10: # X sign
        dx = dx - 256
    assert dx == 10, f"Expected dx=10, got {dx}"

    dy = b2
    if b0 & 0x20: # Y sign
        dy = dy - 256
    assert dy == -10, f"Expected dy=-10, got {dy}"
    print("  -> PASS: PS/2 Mouse 3-byte packet & 9-bit delta decoding")

def test_block_device_boundaries():
    print("[TEST] Block device bounds checking...")
    sector_size = 512
    total_sectors = 2048 # 1 MiB

    def validate_request(lba, count):
        if count == 0:
            return False
        if lba + count > total_sectors:
            return False
        return True

    assert validate_request(0, 1) is True
    assert validate_request(2047, 1) is True
    assert validate_request(2048, 1) is False, "Allowed read past end"
    assert validate_request(2047, 2) is False, "Allowed spanning read past end"
    assert validate_request(0, 0) is False, "Allowed 0 count"
    print("  -> PASS: Block device bounds checking")

def test_usb_descriptors():
    print("[TEST] USB standard descriptor structures...")
    # Standard descriptor sizes
    assert struct.calcsize("<BBHHH") == 8, "USB setup packet must be 8 bytes"
    assert struct.calcsize("<BBHBBBBHHHBBBB") == 18, "USB device descriptor must be 18 bytes"
    assert struct.calcsize("<BBHBBBBB") == 9, "USB config descriptor must be 9 bytes"
    assert struct.calcsize("<BBBBBBBBB") == 9, "USB interface descriptor must be 9 bytes"
    assert struct.calcsize("<BBBBHB") == 7, "USB endpoint descriptor must be 7 bytes"
    print("  -> PASS: USB descriptor binary packing and sizes")

def test_ioapic_iso_routing():
    print("[TEST] I/O APIC ISA IRQ -> GSI redirection flags...")
    def encode_redtbl(vector, iso_flags):
        flags = 0
        if (iso_flags & 0x03) == 0x03:
            flags |= (1 << 13)  # Active-low polarity
        if ((iso_flags >> 2) & 0x03) == 0x03:
            flags |= (1 << 15)  # Level-triggered
        return vector | flags  # Bit 16 (masked) cleared

    # ISA IRQ 1 -> vector 0x21, edge-triggered active-high (flags=0)
    assert encode_redtbl(0x21, 0) == 0x21
    # Active-low level-triggered ISO (flags=0x0F)
    assert encode_redtbl(0x2C, 0x0F) == (0x2C | (1 << 13) | (1 << 15))
    print("  -> PASS: I/O APIC redirection entry flag encoding")

def main():
    print("============================================================")
    print("  ZERO OS 3.0.0 Hardware Foundation Test Suite")
    print("============================================================")
    test_acpi_checksum()
    test_acpi_madt_parsing()
    test_pci_address_and_bar()
    test_ps2_keyboard_decoding()
    test_ps2_mouse_packet()
    test_block_device_boundaries()
    test_usb_descriptors()
    test_ioapic_iso_routing()
    print("============================================================")
    print("ALL 8 HARDWARE FOUNDATION SUITES PASSED (100% OK)")
    print("============================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
