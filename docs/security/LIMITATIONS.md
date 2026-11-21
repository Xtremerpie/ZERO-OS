# Security Model Boundaries & Limitations

## 1. Current Architectural Boundaries (Milestone 7.0.0)

While Milestone 7.0.0 implements genuine, defense-in-depth security mechanisms, the following boundaries reflect the current development stage of ZERO OS:

1. **Hardware Cryptographic Acceleration**:
   - The SHA-256 implementation is completely freestanding software C11.
   - Intel SHA-NI (SHA Extensions) and AES-NI instruction set optimizations are scheduled for future microarchitectural tuning.

2. **Asymmetric Public Key Infrastructure (PKI)**:
   - Milestone 7.0.0 uses symmetric SHA-256 cryptographic digests and constant-time HMAC verification for package and token validation.
   - Elliptic Curve Cryptography (Ed25519) and RSA signature checking with X.509 certificate chains are roadmap items for Milestone 8.0.0 / 9.0.0.

3. **Multi-User Graphical Session Switching**:
   - The DAC subsystem supports arbitrary POSIX UID/GID ownership and permission mode bits.
   - Currently, the graphical desktop environment operates in a primary single-user session with daemon sandboxed worker threads. Multi-session simultaneous desktop switching is not yet exposed in the UI.

4. **Hardware TPM & Measured Boot**:
   - Boot verification relies on Limine UEFI bootloader chain validation.
   - TPM 2.0 PCR extension and sealed storage will be implemented in future hardware iterations.

---

## 2. Safety Invariant Guarantee
ZERO OS strictly isolates all filesystem and memory operations during testing to virtualized or memory-resident targets (RAM disk, virtual images). Under no circumstances will testing or deployment touch the host Windows partitions, EFI system partitions, or physical SSD sectors.
