# Secure Package Verification & Trust Roots

## 1. Overview
The ZERO Package Manager (`zpm`) and `.zapp` binary package format are fortified with cryptographic digest validation in Milestone 7.0.0. Every native application package submitted for installation undergoes cryptographic verification prior to extracting executable code or registering system handlers.

---

## 2. Package Binary Layout & Cryptographic Digest
The 128-byte ZAPP header contains a dedicated 32-byte field:
`uint8_t sha256[32];`

```
+--------------------------------------------------------------+
|                    128-Byte ZAPP Header                      |
| Magic (4B) | Version (2B) | App ID (32B) | Target Arch (12B) |
| Offsets & Sizes (Manifest, Entries, Payload) | CRC32 (4B)    |
|                Payload SHA-256 Digest (32B)                  |
+--------------------------------------------------------------+
|                    384-Byte Manifest                         |
| Name, Description, Category, Exec Path, Permissions, OS Ver  |
+--------------------------------------------------------------+
|                 144-Byte Entry Array (1..32)                 |
| Path, Offset, Size, Permissions Mode, Exec/Config Flags      |
+--------------------------------------------------------------+
|                         Raw Payload                          |
| Binaries, Icons, Assets, Configuration Files                 |
+--------------------------------------------------------------+
```

---

## 3. Verification Protocol
During `zpm_install_buffer()` or `zpm_install_file()`:
1. **Magic & Header Validation**: Validates magic `0x5A415050`, format version `1`, target arch `x86_64`, and non-overlapping boundary offsets.
2. **Payload Cryptographic Hash**: The kernel computes SHA-256 over the contiguous payload slice `[payload_offset .. payload_offset + payload_size]`.
3. **Digest Comparison**: The computed digest is compared against `hdr.sha256` using constant-time `crypto_memcmp_secure()`.
4. **Enforcement Policies**:
   - In **Enforced Mode** (`zpm_is_signature_enforced() == true`), packages with null digests or mismatched hashes are immediately rejected with `ZAPP_ERR_CHECKSUM_MISMATCH`.
   - In **Advisory Mode**, unsigned community packages trigger an `AUDIT_EVENT_PACKAGE_VERIFY` warning before installation.
5. **DAC Permissions Assignment**: Extracted executable files are assigned mode `0755` (`PERM_DEFAULT_DIR`), while non-executables receive `0644` (`PERM_DEFAULT_FILE`).
6. **Audit Event**: An `AUDIT_EVENT_PACKAGE_INSTALL` record is appended to the security audit ring buffer.
