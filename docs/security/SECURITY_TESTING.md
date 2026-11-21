# Security Testing & Verification Suite

## 1. Overview
Security correctness in ZERO OS is verified across two complementary tiers:
1. **In-Kernel Boot Self-Tests** (`kernel/debug/selftest_security.c`): Executed during early kernel boot on the real higher-half kernel in Ring 0.
2. **Host-Side Verification Suites** (`tests/test_security.py`): Standalone Python unit tests verifying binary layouts, packing offsets, bitwise mathematics, and cryptographic KAT vectors.

---

## 2. In-Kernel Self-Tests (`security_run_selftests()`)

The boot self-test suite executes 7 automated test suites:
1. `test_crypto_engine()`: Validates FIPS SHA-256 Known Answer Tests for empty string and standard vectors, verifies constant-time memory comparisons, and tests CSPRNG random generation.
2. `test_capability_lifecycle()`: Validates capability handle allocation, type checking, permission bitmasks, and rejection of unauthorized PIDs.
3. `test_capability_attenuation_and_revocation()`: Verifies monotonic subset permissions, prevents privilege escalation attacks, and validates instantaneous token invalidation upon revocation.
4. `test_process_sandboxing()`: Enforces jail root boundary checks, blocks dot-dot directory traversal, verifies network/device gating, and validates canonical user pointer sanitization.
5. `test_filesystem_permissions()`: Validates POSIX-style UID/GID discretionary access control (0777 mode bits) and root superuser bypass.
6. `test_network_firewall()`: Validates packet filter rule priority insertion, port range matching, and default policy enforcement.
7. `test_security_audit()`: Verifies audit ring buffer FIFO order, monotonically increasing IDs, and event persistence.

---

## 3. Host Standalone Suite (`tests/test_security.py`)

Run via Python:
```powershell
& "C:\Users\rajve\AppData\Local\Python\pythoncore-3.14-64\python.exe" tests\test_security.py
```

### Coverage
- **Capability Tests**: Struct packing, handle uniqueness, bitmask constants.
- **Sandbox Tests**: Path traversal regex/containment, 64-bit canonical address bounds.
- **DAC Tests**: Full $8 \times 8$ matrix of POSIX permission modes across Owner, Group, and Other.
- **Crypto Engine Tests**: Multi-block SHA-256 KATs and constant-time execution invariance.
- **Firewall Tests**: Struct byte alignment (80 bytes), rule priority sorting, and multi-port matching.
- **Audit Logging Tests**: Record formatting and schema compliance.
