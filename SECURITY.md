# Security Policy

## 9.0.0 ZERO Ultimate Security & Performance Hardening

In Milestone 9.0.0, performance optimization and low-latency gaming profiles are integrated under strict preservation of all security guarantees:

- **Uncompromised Defense-in-Depth Invariant:**
  - Gaming Mode never disables or weakens kernel memory boundaries ($W \oplus X$, canonical addresses, page table isolation), capability checks, process sandboxing, or packet-filtering firewall rules.
  - A game process runs strictly in unprivileged Ring 3 user mode. A game cannot grant itself ambient authority, bypass VFS jail root path containment, or elevate its privileges.
- **Anti-Starvation Scheduling Defense:**
  - While the low-latency scheduler grants boosted 80 ms time-slices to registered foreground games, it enforces an unyielding limit of at most 4 consecutive boosts (`MAX_CONSECUTIVE_BOOSTS`).
  - Intervening kernel daemons, network workers, and security audit loggers are guaranteed execution slices, preventing denial-of-service through game thread saturation.
- **Auditable System Profile Transitions:**
  - Every transition between `NORMAL`, `GAMING`, and `POWER_SAVER` profiles, as well as game registration and unregistration events, emits an authenticated structured audit record (`AUDIT_EVENT_CONFIG_CHANGE`) into the central audit ring buffer and `/data/log/audit.log`.
- **AI Resource Coordination Security:**
  - AI daemon throttling during gameplay is governed by kernel policy rather than arbitrary client requests.
  - The AI assistant's permission-gated Automation Broker and interactive User Approval Gate remain fully operational.

## 8.0.0 ZERO Intelligence Security Architecture

In Milestone 8.0.0, native artificial intelligence is introduced under a strict zero-trust boundary:

- **Unprivileged Advisory Subsystem:**
  - The AI assistant daemon and model execution engines are strictly unprivileged and cannot directly issue raw system calls or mutate files.
  - All automated operational proposals must be routed through the permission-gated Automation Broker (`kernel/intelligence/ai_automation.c`).
- **Three-Tier Action Risk Taxonomy:**
  - **Tier 1 (Safe / Read-Only):** Non-disruptive telemetry queries (`AI_ACTION_SYS_INFO`) and desktop notifications (`AI_ACTION_NOTIFICATION`) execute with minimal overhead.
  - **Tier 2 (Medium / Process Context):** Application launch (`AI_ACTION_APP_LAUNCH`) and permitted file reads (`AI_ACTION_FILE_READ`) require capability authorization (`CAP_TYPE_PROCESS`, `CAP_TYPE_FILE`).
  - **Tier 3 (Sensitive / State Mutating):** Filesystem writes (`AI_ACTION_FILE_WRITE`) and service controls (`AI_ACTION_SERVICE_CONTROL`) mandate explicit interactive confirmation via the User Approval Gate.
- **Interactive User Approval Gate:**
  - Sensitive operations halt execution and present a desktop authorization prompt requiring explicit `[Approve]` or `[Reject]` actions from the human operator.
  - Rejection immediately drops the action and transitions status to `REJECTED`, preventing replay or bypass.
- **Sandbox & Capability Boundary Integration:**
  - Even when an action is user-approved, the execution engine validates that the target path does not violate the active sandbox jail root path.
  - Process capability tokens are validated via `capability_verify()`. Monotonic attenuation prevents privilege escalation through prompt manipulation.
- **Security Audit Trail:**
  - All AI session lifecycles, model activations, provider switches, prompts, and action approvals/rejections stream directly into the central security audit log buffer (`kernel/security/audit.c`) and `/data/log/audit.log`.

## 7.0.0 ZERO Security Architecture

In Milestone 7.0.0, a complete defense-in-depth security architecture is established across kernel, process, filesystem, cryptographic, and network boundaries:

- **Central Security Dispatcher (`kernel/security/security.c`):**
  - Centralized coordinator managing authorization checks across the kernel.
  - Global enforcement state (`security_is_enforced`) allowing strict zero-trust or advisory operation.
  - Comprehensive telemetry tracking checks total, grants, denials, capabilities, firewall rules, drops, and audit counts.
- **Capability-Based Access Control (`kernel/security/capability.c`):**
  - Ambient authority is replaced with fine-grained, unforgeable 32-bit capability tokens (`cap_handle_t`).
  - Opaque table of 256 kernel-managed entries isolated from Ring 3 modification.
  - Fine-grained permission bitmasks: `READ`, `WRITE`, `EXEC`, `DELETE`, `ADMIN` across 7 resource types (`FILE`, `DIR`, `PROCESS`, `IPC`, `NET_SOCKET`, `DEVICE`, `SYSTEM`).
  - Monotonic delegation attenuation: child processes can only receive equal or reduced permissions; privilege escalation attempts are strictly rejected.
  - Instantaneous revocation invalidating handles across all references immediately.
- **Process Sandboxing & Containment (`kernel/security/sandbox.c`):**
  - Hierarchical runtime containment profiles: `RESTRICTED`, `BASIC`, `STANDARD`, `SYSTEM`.
  - Filesystem jail root path containment preventing directory traversal or out-of-bounds access.
  - Syscall whitelisting blocking prohibited syscalls (e.g. raw sockets under RESTRICTED profile).
  - Strengthened user pointer sanitization (`sandbox_validate_user_ptr`) enforcing canonical 64-bit addresses, non-wrapping arithmetic, and strictly lower-half user space (`< 0x0000800000000000`).
- **Filesystem Discretionary Access Control (`kernel/security/permission.c`):**
  - POSIX-style UID/GID file ownership and octal 0777 permission mode bits (`PERM_MODE_*`).
  - Strict owner, group, and other access authorization checks (`perm_check`) with root superuser bypass.
- **Freestanding Cryptographic Engine (`kernel/security/crypto/`):**
  - Freestanding NIST FIPS 180-4 SHA-256 implementation with boot-time Known Answer Test verification.
  - Constant-time memory comparison (`crypto_memcmp_secure`) mitigating timing side-channel attacks.
  - Hardware-seeded CSPRNG mixing CPU RDTSC and APIC timer jitter entropy.
- **Cryptographic Package Verification (`kernel/apps/zpm.c`):**
  - Mandatory SHA-256 package digest verification in ZPM prior to extraction.
  - Strict signature enforcement toggle (`zpm_is_signature_enforced`).
  - Automatic DAC mode assignment upon package extraction (`0755` for execs, `0644` for files).
- **Packet-Filtering Firewall (`kernel/security/firewall.c`):**
  - 32-entry prioritized packet filtering engine integrated into network ingress (`net_rx`) and egress (`net_tx`).
  - Protocol, direction, IP subnet, and port range matching rules.
  - Default-deny policy toggle supporting strict isolation.
- **Security Audit Logging Subsystem (`kernel/security/audit.c`):**
  - 10 structured security event types capturing authorization failures, grants, revocations, sandbox faults, and firewall drops.
  - Non-blocking 64-entry RAM ring buffer providing zero-allocation telemetry.
  - Persistent `/data/log/audit.log` file appending standard formatted log lines.
- **ZERO Security Center (`kernel/desktop/security_center.c`):**
  - 7-view native graphical security management dashboard with interactive policy controls.
  - Live security telemetry, capability inspector, sandbox policies, firewall rules, and audit trail.

---

## 6.0.0 ZERO Ecosystem Security Architecture

In Milestone 6.0.0, application package security, path traversal defenses, cryptographic integrity, sandbox isolation, and capability verification are established:

- **Path Traversal Security Defenses:**
  - Mandatory relative path validation for all `.zapp` file entries (`zapp_validate_path`).
  - Strict rejection of parent directory segments (`..`), absolute root prefixes (`/` and `\\`), drive letters (`C:`), and non-printable characters across 7 attack vectors.
  - Package extraction enforces isolated destination sandboxes at `/data/apps/<app_id>/`, preventing overwriting of core kernel binaries or system configuration.
- **Dual Cryptographic Package Integrity:**
  - Every `.zapp` package enforces IEEE 802.3 CRC-32 checksum calculation and 256-bit SHA-256 cryptographic digest verification.
  - Corrupted, truncated, or tampered packages fail installation immediately with `ZAPP_ERR_CHECKSUM_MISMATCH`.
- **Capability-Based Permission Model:**
  - Package manifests declare required capability bits (`ZAPP_PERM_WINDOW`, `STORAGE`, `NETWORK`, `DEVICES`, `AUDIO`).
  - Ring 3 user applications operate within user-mode address spaces isolated by 4-level paging and Ring 3 segment descriptors.
- **Application Configuration Isolation:**
  - Persistent preferences are strictly compartmentalized under `/data/config/<app_id>.cfg`. Applications cannot access or overwrite configuration files of other applications.
- **Transactional Package Operations:**
  - Package installations and uninstalls operate with all-or-nothing transactional semantics. Uncompleted extractions are rolled back to preserve filesystem stability.

---

## 5.0.0 ZERO Desktop Security Architecture

In Milestone 5.0.0, graphical boundary enforcement, surface isolation, input sanitization, and honest authentication guarantees are established:

- **Strict Drawing Clipping & Buffer Containment:**
  - Every 2D rendering primitive enforces active surface clipping bounds (`fb->clip`).
  - Window client drawing strictly isolates coordinates within `[win->x + 1, win->y + 26, win->client_w, win->client_h]`. Window overdraw, titlebar tampering, or cross-window pixel corruption is physically impossible.
  - Linear framebuffer access clamps coordinates to hardware pitch and dimensions, preventing MMIO VRAM out-of-bounds writes.
- **Surface Memory & Off-Screen Isolation:**
  - Windows allocate dedicated off-screen memory buffers in system RAM. Unfocused windows cannot intercept or tamper with the client pixels of focused windows.
  - Window reallocation (`wm_resize`) verifies memory allocation and safely cleans up old surfaces without memory leaks.
- **Input Event Clamping & Denial-of-Service Defense:**
  - Mouse coordinates from PS/2 packets are strictly clamped to display boundaries `[0 .. screen_w - 1, 0 .. screen_h - 1]`.
  - Window dragging computes relative deltas and clamps titlebars so windows cannot be dragged entirely off-screen or lost.
  - Window manager maintains a fixed table capacity (`MAX_WINDOWS = 32`), rejecting excess window creation to prevent memory exhaustion attacks.
- **Honest Development Mode Authentication Notice:**
  - The Graphical Login Screen provides an explicit, high-contrast, on-screen notice explaining that the system is operating in Development Authentication Mode (`[ NOTICE: Development Authentication Mode - Press Sign In or Enter ]`).
  - It never falsely claims cryptographic credentials or biometric security when operating in dev authentication mode.
- **Host Safety Guarantee:**
  - All framebuffer allocations, double buffers, window surfaces, and compositor outputs remain strictly within the emulated virtual machine or user-selected graphics device. ZERO OS never writes to host Windows files, EFI partitions, or physical display controllers without explicit user consent.

---

## 4.0.0 Storage & Networking Security Architecture

In Milestone 4.0.0, filesystem integrity, storage boundary verification, packet validation, and network isolation are established:

- **Storage Boundary & Overflow Protection:**
  - Partition boundaries are strictly enforced: all relative I/O requests must satisfy $\text{lba} + \text{count} \le \text{partition\_sectors}$.
  - Arithmetic overflow checking prevents 64-bit integer wraparound attacks on sector calculations.
  - GPT headers and partition arrays require standard IEEE 802.3 CRC32 verification before acceptance; corrupted partition tables are rejected.
- **ZEROFS Invariant & Memory Defenses:**
  - Inode numbers and data block indices are checked against bitmap bounds before access.
  - Filesystem metadata writes pass through the block cache with clean state tracking.
- **RFC 791 IPv4 Checksum Enforcement:**
  - Inbound IPv4 packets undergo mandatory 16-bit one's complement Internet checksum verification; damaged headers are dropped immediately.
  - Header length (`IHL`) and total length fields are bounds-checked against received frame size.
- **Network Packet Buffer (`net_buf`) Integrity:**
  - Zero-copy buffer push and pull operations validate headroom and payload limits, preventing buffer underflows or overflows.
  - Static pool allocation prevents network-driven kernel dynamic memory exhaustion.
- **TCP Sequence & State Machine Guards:**
  - Connection state transitions strictly conform to RFC 793 rules.
  - Receive buffers enforce sliding window boundaries to protect against out-of-order sequence injection.
- **Socket System Call Pointer Sanitization:**
  - Syscalls `SYS_SEND` and `SYS_RECV` enforce `syscall_validate_user_ptr()`, rejecting any user buffers overlapping higher-half kernel space.

---

## 3.0.0 Hardware Foundation Security Architecture

In Milestone 3.0.0, hardware interface validation, safe resource probing, and peripheral isolation are established:

- **Safe ACPI Table Bounds & Checksum Verification:**
  - Every discovered ACPI table is validated against an 8-bit modular checksum (`sum mod 256 == 0`) before its contents are inspected.
  - Table length fields are checked to ensure they meet minimum structure sizes (`length >= sizeof(acpi_sdt_header_t)`), preventing out-of-bounds memory dereferencing.
  - Maximum table count guards (`ACPI_MAX_TABLES = 32`, `ACPI_MAX_CPUS = 32`, `ACPI_MAX_IOAPIC = 4`, `ACPI_MAX_ISO = 16`) prevent unbounded buffer allocation attacks.
- **Non-Destructive PCI BAR Probing:**
  - BAR sizing preserves original hardware register values by reading the original value, writing the sizing mask (`0xFFFFFFFF`), reading the mask, and immediately restoring the original value before returning.
  - Configuration space accesses enforce alignment and validate bus/device/function boundaries.
- **Input Event Queue Bounded Buffering:**
  - Unified input events are queued in a spinlock-protected circular FIFO buffer (`INPUT_QUEUE_SIZE = 128`).
  - Queue overflow automatically drops the oldest event rather than blocking or causing kernel memory exhaustion.
  - PS/2 mouse packet processing enforces the protocol sanity check (bit 3 of byte 0 must be 1) and discards corrupted packets on overflow flags.
- **Block Storage Bounds & Read-Only Protection:**
  - All read/write block operations validate 64-bit LBA offsets with arithmetic overflow checking (`chk_add_u64`).
  - Read/write requests exceeding device capacity are rejected with error code `-1`.
  - Write attempts to read-only devices are blocked with permission error `-2`.
  - Storage tests use isolated virtual disks or memory buffers, guaranteeing that physical host drives are never touched.

---

## 2.0.0 System Architecture Security Architecture

In Milestone 2.0.0, privilege separation, memory boundary protection, and execution isolation are fully established:

- **Ring 0 / Ring 3 Privilege Separation:**
  - Distinct GDT segments for User Data (`0x28`, RPL 3) and User Code (`0x30`, RPL 3).
  - Transition to Ring 3 via atomic `iretq` execution in `enter_user_mode`, enforcing unprivileged CPU execution.
  - TSS `rsp0` dynamically loaded before every user execution slice to guarantee that interrupts and syscalls trap to a clean, isolated kernel execution stack.
- **Strict User Pointer Sanitization (`syscall_validate_user_ptr`):**
  - All pointers passed from user space across the system call boundary are checked against the lower-half canonical user boundary (`< 0x0000800000000000`).
  - Arithmetic overflow checks (`chk_add_u64`) prevent 64-bit integer wraparound attacks into higher-half kernel space.
  - Pointers overlapping kernel addresses immediately abort the syscall with `SYS_ERR_FAULT`.
- **Address Space Isolation (Process Separation):**
  - Every process is allocated its own distinct 4-level PML4 paging table.
  - Kernel memory mappings are isolated in the higher half (`0xFFFF800000000000`+); user spaces have separate lower-half mappings and cannot access neighboring process pages.
  - Address space transitions automatically update the hardware `CR3` register upon context switch.
- **IPC Isolation and Boundary Checking:**
  - Spinlock-protected ring buffers prevent race conditions and cross-process buffer injection.
  - Payloads are strictly bounded to `IPC_MAX_PAYLOAD` (128 bytes), preventing heap or stack corruption.
  - Mailboxes enforce a maximum queue capacity (`IPC_QUEUE_CAP` = 16), protecting against memory exhaustion attacks.
  - Process termination automatically purges mailboxes and reclaims open file descriptors.
- **ELF64 Binary Security Verification:**
  - Header validation mandates `EM_X86_64`, `ELFCLASS64`, `ELFDATA2LSB`, and `ET_EXEC`/`ET_DYN`.
  - All `PT_LOAD` segments are validated to ensure `p_filesz <= p_memsz` and that destination addresses reside strictly in lower-half memory.
  - $W \oplus X$ permissions are preserved: code segments are mapped `R-X` with `NX=0`, while data/BSS segments are mapped `RW-NX` with `NX=1`.

---

## 1.0.0 Core Foundation Security Architecture

In Milestone 1.0.0, the foundation for kernel memory protection and privilege isolation is established:
- **$W \oplus X$ (Write XOR Execute) Memory Protection:** Enforced by the Virtual Memory Manager (VMM) on all live page tables.
- **Double-Fault IST Stack Isolation:** Double Fault (`#DF`) executes on a dedicated 16 KiB IST1 stack configured via the TSS.
- **Canonical Address Space Enforcement:** All virtual addresses mapped or translated by the VMM are strictly validated against 48-bit canonical addressing rules.
- **Heap Corruption & Overflow Defense:** Canaries (`0x5A45524F48454150ULL`, `0x43414E4152593437ULL`), double-free detection, and buffer overflow traps.
- **IRQ-Safe Concurrency:** Spinlocks provide `spin_lock_irqsave` to guarantee atomic state preservation without deadlocks or interrupt race conditions.

---

- Threat model and planned mitigations: `docs/THREAT_MODEL.md`.
- Security testing must use authorized, isolated environments (QEMU). Never test against systems you do not own.
- No secrets, API keys or credentials may be committed or embedded in the OS (`.gitignore` blocks common key files).
