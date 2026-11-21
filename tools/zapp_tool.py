#!/usr/bin/env python3
"""
ZERO OS Application Package (.zapp) Tool (Milestone 6.0.0 ZERO Ecosystem)

Creates, validates, inspects, and extracts native .zapp packages.
"""

import sys
import os
import struct
import binascii
import hashlib
import argparse

ZAPP_MAGIC = 0x5A415050
ZAPP_FORMAT_VER = 1
ZAPP_MAX_PATH = 128
ZAPP_MAX_ENTRIES = 32

# zapp_header_t: 128 bytes
# uint32 magic, uint16 format_ver, uint16 flags, char[32] app_id, char[16] app_version,
# char[12] target_arch, uint32 manifest_off, uint32 manifest_sz, uint32 entry_count,
# uint32 entries_off, uint32 payload_off, uint32 payload_sz, uint32 checksum, uint8[32] sha256
HEADER_FORMAT = "<IHH32s16s12sIIIIIII32s"
HEADER_SIZE = 128

# zapp_manifest_t: 384 bytes
# char[64] name, char[128] desc, char[32] category, char[32] author, char[16] license,
# char[64] exec_path, char[32] icon_path, uint32 permissions, uint32 min_os, uint8[8] reserved
MANIFEST_FORMAT = "<64s128s32s32s16s64s32sII8s"
MANIFEST_SIZE = 384

# zapp_file_entry_t: 144 bytes
# char[128] path, uint32 offset, uint32 size, uint32 mode, uint32 flags
ENTRY_FORMAT = "<128sIIII"
ENTRY_SIZE = 144

def validate_path(path):
    if not path or len(path) >= ZAPP_MAX_PATH:
        return False, "Path empty or exceeds max length"
    if path.startswith("/") or path.startswith("\\"):
        return False, "Absolute paths prohibited (path traversal protection)"
    parts = path.replace("\\", "/").split("/")
    if ".." in parts:
        return False, "Relative parent navigation '..' prohibited (path traversal protection)"
    for ch in path:
        if ord(ch) < 32 or ord(ch) > 126 or ch == ':':
            return False, f"Invalid character in path: {ch}"
    return True, "OK"

def create_package(output_path, app_id, version, name, desc, category, author, license_name,
                   exec_path, files):
    manifest_bytes = struct.pack(
        MANIFEST_FORMAT,
        name.encode('utf-8')[:64],
        desc.encode('utf-8')[:128],
        category.encode('utf-8')[:32],
        author.encode('utf-8')[:32],
        license_name.encode('utf-8')[:16],
        exec_path.encode('utf-8')[:64],
        b"", # icon_path
        7,   # permissions: window | storage | network
        0x060000, # 6.0.0
        b"\x00" * 8
    )

    manifest_off = HEADER_SIZE
    entries_off = manifest_off + len(manifest_bytes)
    entry_count = len(files)
    payload_off = entries_off + (entry_count * ENTRY_SIZE)

    entries_bytes = bytearray()
    payload_bytes = bytearray()
    curr_offset = 0

    # Validate all paths first
    paths_seen = set()
    for rel_path, data in files:
        ok, reason = validate_path(rel_path)
        if not ok:
            raise ValueError(f"Invalid path '{rel_path}': {reason}")
        if rel_path in paths_seen:
            raise ValueError(f"Duplicate path in package: '{rel_path}'")
        paths_seen.add(rel_path)

        f_size = len(data)
        flags = 1 if rel_path == exec_path else 0
        entry_b = struct.pack(ENTRY_FORMAT, rel_path.encode('utf-8')[:128], curr_offset, f_size, 0o755, flags)
        entries_bytes.extend(entry_b)
        payload_bytes.extend(data)
        curr_offset += f_size

    # Integrity calculation: CRC32 and SHA256 of payload
    crc = binascii.crc32(payload_bytes) & 0xFFFFFFFF
    sha = hashlib.sha256(payload_bytes).digest()

    hdr_bytes = struct.pack(
        HEADER_FORMAT,
        ZAPP_MAGIC,
        ZAPP_FORMAT_VER,
        0, # flags
        app_id.encode('utf-8')[:32],
        version.encode('utf-8')[:16],
        b"x86_64",
        manifest_off,
        len(manifest_bytes),
        entry_count,
        entries_off,
        payload_off,
        len(payload_bytes),
        crc,
        sha
    )

    with open(output_path, "wb") as f:
        f.write(hdr_bytes)
        f.write(manifest_bytes)
        f.write(entries_bytes)
        f.write(payload_bytes)

    return True

def validate_package(pkg_path):
    with open(pkg_path, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        return False, "File smaller than ZAPP header"

    hdr_vals = struct.unpack(HEADER_FORMAT, data[:HEADER_SIZE])
    magic, fmt_ver, flags, app_id, app_ver, target_arch, m_off, m_sz, e_count, e_off, p_off, p_sz, crc, sha = hdr_vals

    if magic != ZAPP_MAGIC:
        return False, f"Invalid magic: 0x{magic:08X} (expected 0x5A415050)"
    if fmt_ver != ZAPP_FORMAT_VER:
        return False, f"Unsupported format version: {fmt_ver}"
    if target_arch.rstrip(b'\x00').decode('utf-8') != "x86_64":
        return False, f"Incompatible architecture: {target_arch}"

    if p_off + p_sz > len(data):
        return False, "Payload exceeds file size (corrupted package)"

    payload = data[p_off:p_off+p_sz]
    calc_crc = binascii.crc32(payload) & 0xFFFFFFFF
    if calc_crc != crc:
        return False, f"CRC32 mismatch: got 0x{calc_crc:08X}, expected 0x{crc:08X}"

    calc_sha = hashlib.sha256(payload).digest()
    if calc_sha != sha:
        return False, "SHA256 digest mismatch"

    # Validate entries
    entries_data = data[e_off:e_off + e_count * ENTRY_SIZE]
    seen_paths = set()
    for i in range(e_count):
        e_bytes = entries_data[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE]
        e_path, e_off_p, e_sz_p, e_mode, e_flags = struct.unpack(ENTRY_FORMAT, e_bytes)
        rel_path = e_path.rstrip(b'\x00').decode('utf-8')
        ok, reason = validate_path(rel_path)
        if not ok:
            return False, f"Invalid entry path '{rel_path}': {reason}"
        if rel_path in seen_paths:
            return False, f"Duplicate entry path: '{rel_path}'"
        seen_paths.add(rel_path)
        if e_off_p + e_sz_p > p_sz:
            return False, f"Entry '{rel_path}' exceeds payload bounds"

    return True, "Valid ZAPP package"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: zapp_tool.py <validate|inspect> <pkg.zapp>")
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "validate":
        ok, msg = validate_package(sys.argv[2])
        print(f"[{'PASS' if ok else 'FAIL'}] {msg}")
        sys.exit(0 if ok else 1)
    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)
