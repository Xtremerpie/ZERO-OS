# ZERO OS 7.0.0 — Security Threat Model

**Milestone:** 7.0.0 ZERO Security  
**Status:** Official Security Reference  
**Scope:** Kernel, User-Space Applications, Storage, Network, and Distribution Platform  

---

## 1. Introduction & Trust Boundaries

The ZERO OS security architecture establishes strict trust boundaries between components based on privilege levels and cryptographic verification:

```
+-------------------------------------------------------------------------+
| UNTRUSTED EXTERNAL ENVIRONMENT (Network Packets, Untrusted Storage)     |
+-------------------------------------------------------------------------+
                                 │ Firewall & Packet Filter (Phase 7)
                                 ▼
+-------------------------------------------------------------------------+
| RING 3 APPLICATION LAYER (Third-Party Apps, SDK Binaries, UI Clients)   |
| Confined by: Process Sandboxing (Phase 3) & Capabilities (Phase 2)      |
+-------------------------------------------------------------------------+
                                 │ Fast Syscall Boundary (Pointer Validation)
                                 ▼
+-------------------------------------------------------------------------+
| RING 0 MICROKERNEL CORE (VMM, PMM, Sched, VFS, Drivers, Security Core)  |
| Trusted Computing Base (TCB) - Centralized Security Monitor (Phase 1)   |
+-------------------------------------------------------------------------+
                                 ▲
                                 │ Cryptographic Verification (Phase 5 & 6)
+-------------------------------------------------------------------------+
| TRUSTED ROOT OF TRUST (Embedded Keyring, UEFI Limine Bootloader)        |
+-------------------------------------------------------------------------+
```

---

## 2. Threat Catalog & Mitigations

### 2.1 Threat 1: Untrusted or Malicious Applications
- **Threat:** An application attempts to read sensitive files, spy on other applications, or corrupt kernel memory.
- **Mitigation:**
  - Capability-based security: applications must possess an explicit kernel capability to access any resource.
  - Process sandboxing: applications run under sandbox profiles (`BASIC`, `STANDARD`, `RESTRICTED`) that chroot VFS access and restrict IPC.
  - Memory isolation: W^X page protection and distinct PML4 page tables prevent cross-process memory access.

### 2.2 Threat 2: Malicious or Corrupted Packages (`.zapp`)
- **Threat:** An attacker tampers with a package to inject malicious code or performs path traversal to overwrite system binaries.
- **Mitigation:**
  - Dual cryptographic verification: SHA-256 payload digest and digital signature verification against trusted root keys.
  - Mandatory path traversal filtering: package extraction strictly prohibits relative parent directories (`..`), leading slashes, and drive colons.
  - Sandboxed installation: packages are unpacked exclusively into `/data/apps/<app_id>/`.

### 2.3 Threat 3: Malformed or Hostile System Calls
- **Threat:** Ring 3 application passes kernel-space addresses, null pointers, or overlapping ranges to trigger kernel faults or privilege escalation.
- **Mitigation:**
  - Comprehensive user pointer sanitization: `syscall_validate_user_ptr` verifies that addresses reside strictly in canonical user address space and do not wrap around into higher-half kernel space (`0xFFFF800000000000`).
  - Kernel memory canaries and safe copying routines.

### 2.4 Threat 4: Unauthorized Filesystem Access
- **Threat:** Process attempts to access files owned by system services or another application.
- **Mitigation:**
  - Filesystem DAC (Discretionary Access Control): File ownership (UID/GID) and mode permission bits (`rwxrwxrwx`).
  - Capability-directed VFS descriptors.

### 2.5 Threat 5: Network Abuse & Port Scanning
- **Threat:** Unauthorized network connections or inbound attacks on open socket ports.
- **Mitigation:**
  - Kernel packet-filtering firewall with configurable allow/deny rules and default-deny option.
  - Capability requirement (`CAP_TYPE_NET_SOCKET`) for opening sockets.
  - Network security audit logging for dropped packets.

### 2.6 Threat 6: Tampering with Security Audit Trail
- **Threat:** Malicious process modifies or clears audit logs to conceal unauthorized actions.
- **Mitigation:**
  - The security audit log is maintained in kernel-managed memory and append-only disk storage. Ring 3 user processes have zero write access to audit log structures.

---

## 3. Residual Threats Outside Current Scope

The following threats are explicitly acknowledged as beyond the scope of Milestone 7.0.0:
1. **Hardware Side-Channel Attacks** (Spectre, Meltdown, MDS on physical x86 CPUs): requires microcode updates and complex branch target isolation not applicable in lightweight emulator environments.
2. **Physical Memory Extraction (Cold Boot Attacks)**: requires full-disk AES-XTS encryption and TPM-based hardware key sealing (planned for release hardening).
3. **Formal Verification**: ZERO OS is written in safe C11 and x86_64 assembly, but is not mathematically proven using formal methods.
