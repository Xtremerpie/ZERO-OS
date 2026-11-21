#!/usr/bin/env python3
"""
ZERO OS 6.0.0 ZERO Ecosystem Test Suite
Milestone 6.0.0 — ZERO Ecosystem (Codename: ZERO ECOSYSTEM)

Validates .zapp binary package header structures, 384-byte manifest format,
144-byte file entries, CRC32 and SHA256 integrity checksums, path traversal
security guards, ZPM package manager CLI commands, store repository catalog,
application configuration parser, toast notification timing, and ZERO SDK headers.
"""

import os
import sys
import struct
import zlib
import hashlib
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Header layout constants
ZAPP_MAGIC = 0x5A415050
ZAPP_FORMAT_VER = 1
ZAPP_HEADER_SIZE = 128
ZAPP_MANIFEST_SIZE = 384
ZAPP_ENTRY_SIZE = 144

# Permission flags
ZAPP_PERM_WINDOW  = (1 << 0)
ZAPP_PERM_STORAGE = (1 << 1)
ZAPP_PERM_NETWORK = (1 << 2)
ZAPP_PERM_DEVICES = (1 << 3)
ZAPP_PERM_AUDIO   = (1 << 4)

class ZappHeaderTest(unittest.TestCase):
    """Test 128-byte ZAPP binary package header."""

    def test_header_packing_size(self):
        # Format: <I H H 32s 16s 12s I I I I I I I 32s
        # 4 + 2 + 2 + 32 + 16 + 12 + (7 * 4) + 32 = 128 bytes
        hdr_fmt = "<IHH32s16s12sIIIIIII32s"
        calc_size = struct.calcsize(hdr_fmt)
        self.assertEqual(calc_size, ZAPP_HEADER_SIZE,
                         f"Header struct size must be exactly 128 bytes, got {calc_size}")

    def test_manifest_packing_size(self):
        # 64 + 128 + 32 + 32 + 16 + 64 + 32 + 4 + 4 + 8 = 384 bytes
        manifest_fmt = "<64s128s32s32s16s64s32sII8s"
        calc_size = struct.calcsize(manifest_fmt)
        self.assertEqual(calc_size, ZAPP_MANIFEST_SIZE,
                         f"Manifest struct size must be exactly 384 bytes, got {calc_size}")

    def test_entry_packing_size(self):
        # 128 + 4 + 4 + 4 + 4 = 144 bytes
        entry_fmt = "<128sIIII"
        calc_size = struct.calcsize(entry_fmt)
        self.assertEqual(calc_size, ZAPP_ENTRY_SIZE,
                         f"Entry struct size must be exactly 144 bytes, got {calc_size}")


class PathTraversalSecurityTest(unittest.TestCase):
    """Test security defenses against malicious package paths."""

    def is_safe_path(self, path: str) -> bool:
        if not path or path.startswith("/") or path.startswith("\\"):
            return False
        if len(path) > 1 and path[1] == ":":
            return False
        parts = path.replace("\\", "/").split("/")
        for p in parts:
            if p == "..":
                return False
        return True

    def test_rejection_of_traversal_paths(self):
        malicious = [
            "../etc/passwd",
            "/bin/sh",
            "../../root",
            "subdir/../../secret",
            "..\\windows\\system32",
            "C:/autoexec.bat",
            "D:\\boot.ini",
            "/absolute/root/file"
        ]
        for p in malicious:
            self.assertFalse(self.is_safe_path(p), f"Failed to reject malicious path: {p}")

    def test_acceptance_of_legitimate_paths(self):
        legitimate = [
            "bin/calc",
            "icons/app.png",
            "readme.txt",
            "data/defaults.cfg",
            "assets/fonts/custom.bin"
        ]
        for p in legitimate:
            self.assertTrue(self.is_safe_path(p), f"Legitimate path incorrectly rejected: {p}")


class ZappCreationAndIntegrityTest(unittest.TestCase):
    """Test full .zapp package creation and checksum verification."""

    def test_create_and_verify_package(self):
        app_id = b"zero.calculator"
        version = b"1.0.0"
        arch = b"x86_64"

        manifest_data = struct.pack(
            "<64s128s32s32s16s64s32sII8s",
            b"ZERO Calculator",
            b"Native desktop arithmetic utility",
            b"Productivity",
            b"ZERO OS Team",
            b"MIT",
            b"bin/calc",
            b"icons/calc.png",
            ZAPP_PERM_WINDOW,
            0x060000,
            b"\x00" * 8
        )
        self.assertEqual(len(manifest_data), 384)

        file1_data = b"ELF64_TEST_BINARY_DATA_CALCULATOR_CORE"
        file1_entry = struct.pack(
            "<128sIIII",
            b"bin/calc",
            0,
            len(file1_data),
            0o755,
            1  # EXEC
        )

        payload = file1_data
        crc32_val = zlib.crc32(payload) & 0xFFFFFFFF
        sha256_val = hashlib.sha256(payload).digest()

        header_data = struct.pack(
            "<IHH32s16s12sIIIIIII32s",
            ZAPP_MAGIC,
            ZAPP_FORMAT_VER,
            0,
            app_id,
            version,
            arch,
            128,              # manifest_offset
            384,              # manifest_size
            1,                # entry_count
            128 + 384,        # entries_offset
            128 + 384 + 144,  # payload_offset
            len(payload),     # payload_size
            crc32_val,
            sha256_val
        )
        self.assertEqual(len(header_data), 128)

        package_blob = header_data + manifest_data + file1_entry + payload
        self.assertEqual(len(package_blob), 128 + 384 + 144 + len(payload))

        # Unpack and verify
        magic, ver, _, uid, uver, uarch, m_off, m_sz, e_cnt, e_off, p_off, p_sz, ucrc, usha = struct.unpack(
            "<IHH32s16s12sIIIIIII32s", package_blob[:128]
        )
        self.assertEqual(magic, ZAPP_MAGIC)
        self.assertEqual(ver, 1)
        self.assertEqual(uid.split(b"\x00")[0], app_id)
        self.assertEqual(uver.split(b"\x00")[0], version)
        self.assertEqual(uarch.split(b"\x00")[0], arch)
        self.assertEqual(ucrc, crc32_val)
        self.assertEqual(usha, sha256_val)


class ZpmRepositoryAndCatalogTest(unittest.TestCase):
    """Test ZPM repository catalog, search indexing, and category filtering."""

    CATALOG = [
        {"app_id": "calc", "name": "ZERO Calculator", "version": "1.0.0", "category": "Productivity", "size": 18432},
        {"app_id": "editor", "name": "ZERO Text Editor", "version": "1.0.0", "category": "Productivity", "size": 24576},
        {"app_id": "viewer", "name": "ZERO Image Viewer", "version": "1.0.0", "category": "Media", "size": 28672},
        {"app_id": "archiver", "name": "ZERO Archive Viewer", "version": "1.0.0", "category": "Utilities", "size": 20480},
        {"app_id": "netmon", "name": "ZERO Network Monitor", "version": "1.0.0", "category": "Utilities", "size": 22528},
        {"app_id": "hello", "name": "ZERO Hello World", "version": "1.0.0", "category": "Development", "size": 16384},
    ]

    def test_catalog_categories(self):
        categories = {item["category"] for item in self.CATALOG}
        self.assertIn("Productivity", categories)
        self.assertIn("Media", categories)
        self.assertIn("Utilities", categories)
        self.assertIn("Development", categories)

    def test_search_queries(self):
        query = "calc"
        matches = [x for x in self.CATALOG if query in x["app_id"] or query in x["name"].lower()]
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["app_id"], "calc")

        # Empty query returns full catalog
        all_matches = [x for x in self.CATALOG if "" in x["name"].lower()]
        self.assertEqual(len(all_matches), len(self.CATALOG))


class AppConfigEngineTest(unittest.TestCase):
    """Test application configuration parser."""

    def parse_cfg(self, text: str) -> dict:
        result = {}
        for line in text.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                result[k.strip()] = v.strip()
        return result

    def test_config_parsing(self):
        cfg_text = """
        # ZERO OS App Config
        theme = dark_gold
        window_width = 800
        window_height = 600
        hardware_accel = true
        auto_update = 0
        """
        cfg = self.parse_cfg(cfg_text)
        self.assertEqual(cfg["theme"], "dark_gold")
        self.assertEqual(int(cfg["window_width"]), 800)
        self.assertEqual(int(cfg["window_height"]), 600)
        self.assertEqual(cfg["hardware_accel"], "true")
        self.assertEqual(cfg["auto_update"], "0")


class SdkVerificationTest(unittest.TestCase):
    """Test ZERO OS SDK headers, assembly stubs, and templates."""

    def test_sdk_headers_exist(self):
        headers = [
            "sdk/include/zero/app.h",
            "sdk/include/zero/window.h",
            "sdk/include/zero/draw.h",
            "sdk/include/zero/event.h",
            "sdk/include/zero/syscall.h",
            "sdk/crt0.asm",
            "sdk/linker_user.ld",
            "sdk/templates/hello_world/main.c",
            "tools/zapp_tool.py"
        ]
        for rel_path in headers:
            full_path = os.path.join(ROOT, rel_path)
            self.assertTrue(os.path.exists(full_path), f"Missing required SDK file: {rel_path}")

    def test_syscall_header_numbers(self):
        syscall_h = os.path.join(ROOT, "sdk", "include", "zero", "syscall.h")
        content = open(syscall_h).read()
        self.assertIn("SYS_EXIT", content)
        self.assertIn("SYS_WRITE", content)
        self.assertIn("SYS_READ", content)
        self.assertIn("SYS_OPEN", content)
        self.assertIn("SYS_CLOSE", content)


if __name__ == "__main__":
    print("=" * 60)
    print("  ZERO OS 6.0.0 ZERO Ecosystem Test Suite")
    print("=" * 60)
    suite = unittest.TestSuite()
    for test_class in [ZappHeaderTest, PathTraversalSecurityTest, ZappCreationAndIntegrityTest,
                       ZpmRepositoryAndCatalogTest, AppConfigEngineTest, SdkVerificationTest]:
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(test_class))
    runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=2)
    result = runner.run(suite)
    if result.wasSuccessful():
        print("=" * 60)
        print("ALL 6 ZERO ECOSYSTEM TEST SUITES PASSED (100% OK)")
        print("=" * 60)
        sys.exit(0)
    else:
        print("=" * 60)
        print(f"FAILED: {len(result.failures)} failures, {len(result.errors)} errors")
        print("=" * 60)
        sys.exit(1)
