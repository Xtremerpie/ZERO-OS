# Changelog

Only the ten official versions appear as headings (`VERSION_POLICY.md`). Internal builds are not listed.

## 9.0.0 — ZERO Ultimate (COMPLETE: ZERO Gaming Mode, Low-Latency Scheduler Profile, Graphics Device Abstraction & Hardware Detection, Audio Subsystem, Native "ZERO Strike" Demo Game, ZERO Gaming Control Center GUI, AI Resource Coordination, Repeatable Performance Benchmark Suite, Release Hardening)

### Added (IMPLEMENTED, TESTED)
- **ZERO Gaming Mode (`kernel/gaming/gaming.*`):**
  - Dedicated system performance profiles: `NORMAL`, `GAMING`, and `POWER_SAVER`.
  - Dynamic game process registration (`gaming_register_game`) and automatic profile restoration on exit (`gaming_unregister_game`).
  - Configuration persistence in `/data/config/gaming.cfg`.
  - Integration with ZERO Security: all profile transitions audited via `audit_log()`.
- **Low-Latency Preemptive Scheduler (`kernel/process/scheduler.*`):**
  - Dynamic time-slice scaling: 80 ms foreground game process slice vs. 20 ms background slice in gaming profile.
  - Ready queue priority dequeue for foreground game threads with a strict 4x anti-starvation ceiling (`MAX_CONSECUTIVE_BOOSTS`).
  - Scheduler latency and context switch diagnostics in `struct scheduler_stats`.
- **Graphics Device Abstraction & Hardware Discovery (`kernel/graphics/gpu.*`):**
  - Unified `gpu_dev_t` interface with capability flags (`GPU_CAP_2D_BLIT`, `GPU_CAP_PAGE_FLIP`, `GPU_CAP_VSYNC`, `GPU_CAP_HARDWARE_CURSOR`, `GPU_CAP_COMMAND_RING`).
  - PCI display controller identification for Intel HD Graphics 500 (`0x8086:0x5A85`), VirtIO-GPU (`0x1AF4:0x1050`), Bochs VGA (`0x1234:0x1111`), and VMware SVGA.
  - Optimized 64-bit SIMD software blitter fallback achieving 1,597.2 FPS (490.65 MPixels/s) on standard surfaces.
- **Audio Device Subsystem (`kernel/drivers/audio.*`):**
  - Audio hardware discovery for Intel High Definition Audio (HDA) and AC'97 controllers.
  - 16-bit signed stereo PCM streaming at 44.1 kHz / 48 kHz into an 8,192-byte ring buffer.
  - Master volume control (0 - 100%), mute toggle, underrun tracking, and safe software null sink fallback.
- **Native Demo Game: "ZERO Strike" (`kernel/desktop/demo_game.*`):**
  - 60 FPS vector space arcade game running natively on the ZERO Desktop window manager.
  - Real-time keyboard input (A/D/Left/Right arrows to move, Space to fire, R to reset, ESC to exit).
  - Parallax starfield, alien invaders, crimson laser projectiles, and real-time performance HUD overlay.
  - Automatically triggers Gaming Mode profile activation on launch and restoration on exit.
- **ZERO Gaming Control Center GUI (`kernel/desktop/gaming_control.*`):**
  - 620x420 native windowed management application with 5 comprehensive views: Profiles, Scheduler, Graphics, Audio, AI Sync.
  - Interactive profile switching buttons, real-time telemetry, and hardware capability status.
- **AI Resource Coordination (`kernel/intelligence/ai_service.*`):**
  - Throttles and pauses background AI assistant inference jobs during gameplay to eliminate CPU contention.
  - Strictly preserves memory budget ceilings ($\le 256$ MB) and security boundary validations.
- **Repeatable Benchmark Suite & Performance Report (`tests/benchmark_suite.py`, `docs/release/PERFORMANCE_REPORT.md`):**
  - Empirical micro-benchmarks measuring memory allocation (665k ops/s), SHA-256 (412 MB/s), scheduler context switch (5.36M ops/s), 2D blitter (1597 FPS), and block cache lookups (3.47M ops/s).
- **Diagnostics & Self-Tests (`kernel/debug/selftest_ultimate.*`, `tests/test_ultimate.py`):**
  - In-kernel 7-suite boot diagnostic test (`ultimate_run_selftests()`) and structured report (`ultimate_report_generate()`).
  - Standalone automated test suite (`tests/test_ultimate.py`) passing 100% (12 unit tests across 6 test classes in 0.007s).

## 8.0.0 — ZERO Intelligence (COMPLETE: ZERO AI Assistant Daemon, Pluggable Provider Abstraction, Local Model Manager, Permission-Gated Automation Broker, Assistant Request Engine, Persistent Memory Store, Security Audit Telemetry, Native 6-View Desktop Assistant Application, Developer C SDK, In-Kernel Self-Tests)

### Added (IMPLEMENTED, TESTED)
- **Central AI Service Coordinator (`kernel/intelligence/ai_service.*`):**
  - Unified subsystem lifecycle orchestration, operational state machine, and global telemetry tracking (`ai_service_stats_t`).
  - Coordinated shutdown, restart, and fault isolation across daemon, model manager, providers, and broker.
- **Native AI Assistant Daemon (`kernel/intelligence/ai_daemon.*`):**
  - Event-driven background service with multi-state operational machine (`IDLE`, `BUSY_INFERENCE`, `AWAITING_APPROVAL`, `EXECUTING_ACTION`, `STOPPED`).
  - Zero-allocation 32-entry circular diagnostic log buffer with error tagging and uptime tracking.
  - Multi-session request queueing and synchronous/asynchronous prompt submission.
- **Pluggable AI Provider Architecture (`kernel/intelligence/ai_provider.*`):**
  - Unified `ai_provider_ops_t` interface decoupling assistant business logic from model execution backends.
  - Local CPU Micro-Runtime (`AI_PROVIDER_TYPE_LOCAL_CPU`) running freestanding AVX2/SSE4.2 quantized micro-models.
  - Host Development Adapter (`AI_PROVIDER_TYPE_ADAPTER`) bridging development environments with host accelerators.
  - Remote Reference Proxy (`AI_PROVIDER_TYPE_REMOTE`) providing socket-based standard REST/gRPC integration over BSD sockets.
- **Local Model Discovery & Lifecycle Manager (`kernel/intelligence/ai_model.*`):**
  - Discovery scanner for persistent models stored in `/data/models/` supporting GGUF and native ZERO Micro-Weights (`.zmw`).
  - Strict physical memory budget enforcement ($\le 256$ MB) preventing system memory exhaustion on 4 GB RAM / Intel Celeron J3355 hardware.
  - Deterministic Single-Active-Model policy: loading a target model automatically invalidates and unloads the previously active model.
- **Permission-Gated Automation Broker (`kernel/intelligence/ai_automation.*`):**
  - Three-tier action risk taxonomy: Tier 1 (Low: SysInfo, Notifications), Tier 2 (Medium: App Launch, File Read), Tier 3 (High: File Write, Service Control).
  - Strict human-in-the-loop User Approval Gate: Tier 3 and sensitive operations block until interactive user consent is granted in the Desktop UI.
  - Full capability checking (`CAP_TYPE_PROCESS`, `CAP_TYPE_FILE`, `CAP_TYPE_SYSTEM`) and sandbox jail root path containment.
- **Assistant Request Lifecycle Engine (`kernel/intelligence/ai_engine.*`):**
  - Conversational multi-turn session tracking (`ai_session_t`) with bounded 16-turn context window preventing memory bloat.
  - Natural language intent classification and structured action extraction (e.g. app launch, file inspection, system metrics).
- **Persistent Conversational Memory & Preferences (`kernel/intelligence/ai_memory.*`):**
  - Persistent user assistant preferences schema in `/data/ai/preferences.cfg`.
  - Timestamped session transcripts stored in `/data/ai/history/session_<id>.log`.
  - Privacy-preserving atomic history purge operation.
- **Security Audit Telemetry Bridge (`kernel/intelligence/ai_audit.*`):**
  - Direct integration of AI prompts, model loads, provider switches, and action proposals into the central ZERO Security audit logger.
  - Structured, tamper-evident audit log entries preserving forensic accountability.
- **Native ZERO AI Assistant Desktop Application (`kernel/desktop/ai_assistant.*`):**
  - 640x440 native windowed GUI featuring 6 dedicated management tabs: Chat, Models, Automation, Memory, Settings, Service.
  - Interactive prompt submission, real-time response rendering, one-click action Approve/Reject buttons, model switching, and service controls.
  - Registered in desktop shell registry (`APP_ID_AI_ASSISTANT`) with taskbar launcher icon.
- **Developer C SDK & Template Demo (`sdk/include/zero/ai.h`, `sdk/templates/ai_hello_world/`):**
  - Complete Ring 3 C API for third-party AI integration: session lifecycle, prompt submission, action proposals, telemetry queries.
  - Complete template demo demonstrating AI session handling and automated desktop notification.
- **Diagnostics & Test Suites (`kernel/debug/selftest_intelligence.*`, `tests/test_intelligence.py`):**
  - In-kernel 7-suite boot diagnostic test (`intelligence_run_selftests()`) and structured report (`intelligence_report_generate()`).
  - Standalone automated test suite (`tests/test_intelligence.py`) passing 100% (13 unit tests across 7 test classes in 0.002s).

## 7.0.0 — ZERO Security (COMPLETE: Central Security Architecture, Capability Access Control, Process Sandboxing, Filesystem DAC, Cryptographic Foundation, Package Verification, Packet-Filtering Firewall, Security Audit Logging, ZERO Security Center)

### Added (IMPLEMENTED, TESTED)
- **Central Security Architecture (`kernel/security/security.*`):**
  - Centralized security decision coordinator, authorization dispatcher, and telemetry tracker (`sec_stats_t`).
  - Runtime global enforcement toggle (`security_is_enforced`, `security_set_enforced`) supporting strict and advisory modes.
- **Capability-Based Access Control (`kernel/security/capability.*`):**
  - Fine-grained unforgeable 32-bit capability tokens (`cap_handle_t`) indexing a 256-entry kernel-managed table.
  - Resource types (`FILE`, `DIR`, `PROCESS`, `IPC`, `NET_SOCKET`, `DEVICE`, `SYSTEM`) and permission bitmasks (`READ`, `WRITE`, `EXEC`, `DELETE`, `ADMIN`).
  - Monotonic delegation attenuation (`cap_duplicate_reduced`) strictly preventing privilege escalation.
  - Immediate token revocation (`cap_revoke`) terminating all future authority.
  - Process capability enumeration for security inspection.
- **Process Sandboxing & Isolation Engine (`kernel/security/sandbox.*`):**
  - Hierarchical containment profiles: `RESTRICTED` (minimal IO, strict jail root, no sockets), `BASIC`, `STANDARD`, `SYSTEM`.
  - Filesystem jail root enforcement (`sandbox_check_path`) preventing directory traversal and out-of-jail access.
  - Syscall access filtering (`sandbox_check_syscall`) integrated directly into syscall dispatcher.
  - Strengthened user-space pointer validator (`sandbox_validate_user_ptr`) checking non-wrapping boundaries, canonical addresses, and strictly lower-half user space (`< 0x0000800000000000`).
- **Filesystem Discretionary Access Control (`kernel/security/permission.*`):**
  - POSIX-compatible UID/GID file ownership and octal 0777 permission mode bits (`PERM_MODE_*`).
  - Strict owner, group, and other access authorization checks (`perm_check`) and root superuser bypass.
- **Cryptographic Foundation (`kernel/security/crypto/`):**
  - Freestanding C11 implementation of NIST FIPS 180-4 SHA-256 with boot-validated Known Answer Tests (KATs).
  - Constant-time memory comparison (`crypto_memcmp_secure`) mitigating timing side-channel attacks.
  - Hardware-seeded CSPRNG (`crypto_random_bytes`) mixing CPU RDTSC and APIC timer jitter entropy.
- **Secure Package Verification (`kernel/apps/zpm.*`):**
  - Cryptographic payload SHA-256 digest validation against header in ZPM.
  - Strict signature enforcement toggle (`zpm_is_signature_enforced`, `zpm_set_signature_enforced`).
  - Automatic DAC mode assignment upon package file extraction (`0755` for execs, `0644` for files).
- **Network Security & Stateful Packet-Filtering Firewall (`kernel/security/firewall.*`):**
  - 32-entry prioritized packet filtering engine integrated directly into `net_rx()` and `net_tx()`.
  - Rule parameters: Protocol (TCP/UDP/ICMP/ALL), Direction (IN/OUT/BOTH), IP and CIDR subnet masks, Port ranges.
  - Configurable global default policy (`firewall_set_default_policy`) supporting zero-trust default-deny.
- **Security Audit Logging Subsystem (`kernel/security/audit.*`):**
  - 10 structured security event types (`AUTH_FAILURE`, `CAP_GRANT`, `CAP_REVOKE`, `PERM_DENY`, `SANDBOX_VIOLATION`, `SYSCALL_VIOLATION`, `PACKAGE_VERIFY`, `PACKAGE_INSTALL`, `FIREWALL_DROP`, `CONFIG_CHANGE`).
  - Zero-allocation 64-entry RAM ring buffer providing non-blocking telemetry.
  - Persistent log stream appending formatted entries to `/data/log/audit.log`.
- **ZERO Security Center Graphical Application (`kernel/desktop/security_center.*`):**
  - Native 640x440 graphical security dashboard adhering to Black / Graphite / Gold / Crimson aesthetics.
  - 7 comprehensive views: *Overview*, *Caps & DAC*, *Sandboxing*, *Firewall*, *Packages*, *Audit Trail*, *Protection*.
  - Live interactive toggles for firewall default policy and package signature enforcement.
  - Integrated into desktop application framework, taskbar launcher, and shell registry (12 native apps).
- **Security Diagnostics & Self-Tests:**
  - In-kernel self-test suite (`kernel/debug/selftest_security.*`) running 7 comprehensive validation suites at boot.
  - Standalone host unit test suite (`tests/test_security.py`) validating bitmasks, struct sizes, jail math, DAC matrices, and KATs.
  - Comprehensive documentation suite in `docs/security/` (10 documents).

## 6.0.0 — ZERO Ecosystem (COMPLETE: .zapp Package Format, ZPM Package Manager, ZERO Store, Calculator, Text Editor, Image Viewer, Archive Viewer, Network Monitor, Persistent Config, Desktop Notifications, ZERO SDK)

### Added (IMPLEMENTED, TESTED)
- **Native Application Package Format (`.zapp` - `kernel/apps/zapp.*`):**
  - Binary package container format with 128-byte header (`zapp_header_t`), 384-byte manifest (`zapp_manifest_t`), and 144-byte file entries (`zapp_file_entry_t`).
  - Strict path traversal security defenses rejecting `..`, leading slashes `/` and `\\`, and drive colons across 7 attack vectors.
  - Mandatory IEEE 802.3 CRC-32 and 256-bit SHA-256 integrity verification.
  - Capability-based permission bits (`WINDOW`, `STORAGE`, `NETWORK`, `DEVICES`, `AUDIO`).
- **ZERO Package Manager Subsystem & CLI (`kernel/apps/zpm.*`):**
  - Persistent installed package registry at `/data/pkg/installed.db` and isolated sandboxes at `/data/apps/<app_id>/`.
  - Transactional package installation, uninstallation, integrity verification, and repository search.
  - Unified CLI (`zpm list`, `zpm info`, `zpm install`, `zpm uninstall`, `zpm search`, `zpm verify`, `zpm update`).
- **ZERO Software Center / ZERO Store (`kernel/desktop/store.*`):**
  - Native 620x420 graphical application center running inside ZERO Desktop.
  - Category views (*All Apps*, *Productivity*, *Development*, *Utilities*, *Internet*, *Media*, *Installed*).
  - Real-time search query filtering over package names and categories.
  - Dynamic state-aware package cards with one-click Install, Launch, and Uninstall actions.
- **Native Application Suite (`kernel/desktop/`):**
  - **ZERO Calculator (`calc.*`):** 280x360 window, interactive 4x5 keypad grid, arithmetic operations, sign inversion, and overflow safety.
  - **ZERO Text Editor (`editor.*`):** 560x380 window, 20x64 text buffer, cursor navigation, keyboard typing, and VFS file save/load.
  - **ZERO Image Viewer (`viewer.*`):** 500x340 window, SMPTE color bars, geometric crosshair, icon matrix, and 1x-4x zoom scaling.
  - **ZERO Archive Viewer (`archiver.*`):** 540x360 window, .zapp header inspector, manifest viewer, and package extractor.
  - **ZERO Network Monitor (`netmon.*`):** 540x360 window, 3 tabs (Overview, ARP Cache, Sockets), real-time RX/TX packet & byte counters, error metrics, and socket connection inspector.
- **Application Configuration Framework (`kernel/apps/config.*`):**
  - Persistent typed key-value preferences engine storing app configs at `/data/config/<app_id>.cfg`.
  - String, int64, and boolean typed accessors with whitespace trimming and comment preservation.
- **Desktop Toast Notifications (`kernel/desktop/notification.*`):**
  - Non-blocking toast overlays (240x52px) composited in bottom-right corner above taskbar.
  - Accent gold bar, icon glyph, title, message, and animated countdown dismissal timer.
- **ZERO Software Development Kit (SDK) & Build Tools:**
  - Standard headers (`sdk/include/zero/syscall.h`, `app.h`, `window.h`, `draw.h`, `event.h`).
  - x86_64 user startup assembly (`sdk/crt0.asm`) and user linker script (`sdk/linker_user.ld`).
  - Canonical demonstration application: `sdk/templates/hello_world/main.c`.
  - Host packaging and validation tool: `tools/zapp_tool.py`.
- **Desktop Upgrades & Diagnostics:**
  - Application registry expanded to 11 native desktop applications (`kernel/desktop/app.c`).
  - ZERO Terminal updated with native `zpm` CLI command execution.
  - ZERO Files updated with automated file associations (`.zapp`, `.txt`, `.cfg`, `.bmp`).
  - ZERO Control Center updated with dedicated "Permissions" security auditing tab.
  - In-kernel self-tests (`kernel/debug/selftest_ecosystem.*`) and automated Python suite (`tests/test_ecosystem.py`).

## 5.0.0 — ZERO Desktop (COMPLETE: Graphics Architecture, Double Buffering, 2D Vector Primitives, Scalable Typography, Vector Cursor, Window Manager, Compositor, Desktop Shell, Login Screen, Native System Apps)

### Added (IMPLEMENTED, TESTED)
- **Official Visual Identity Palette (`kernel/graphics/color.h`):**
  - Official ZERO OS color palette constants: Deep Void Black (`#08080A`), Desktop Graphite Base (`#121216`), Window/Card Panel (`#1A1A20`), Elevated Surface (`#22222A`), Window Border (`#2E2E3A`), Focused Border Gold (`#D4AF37`), Signature Metallic Gold (`#D4AF37`), Crimson Red (`#C41230`), and Crimson Glow (`#E6194B`).
  - RGB component pack/unpack inline helpers with channel extraction.
- **Linear Framebuffer Architecture & Tear-Free Double Buffer (`kernel/graphics/framebuffer.*`):**
  - Linear framebuffer discovery from Limine UEFI bootloader response.
  - Double buffer allocation in system RAM preventing screen tearing.
  - Dirty bounding-box damage tracking (`fb_mark_damage`) ensuring only modified screen areas are copied to hardware VRAM during `fb_flip()`.
  - Strict hardware boundary clipping (`fb_set_clip`, `fb_reset_clip`) strictly enforced on all draw operations.
  - Safe fallback to frontbuffer if system memory is constrained.
- **2D Vector Rendering Engine (`kernel/graphics/draw.*`):**
  - Fast clipping-enforced pixel write (`draw_pixel`) and 8-bit alpha blending (`draw_blend_pixel`).
  - Bresenham line algorithm (`draw_line`) supporting horizontal, vertical, and all-octant diagonal slopes with integer math.
  - Hollow and filled rectangles (`draw_rect`, `draw_fill_rect`) and rounded rectangles (`draw_rounded_rect`, `draw_fill_rounded_rect`).
  - Midpoint circle algorithm for hollow and filled circles (`draw_circle`, `draw_fill_circle`).
  - Vertical linear gradient interpolator (`draw_gradient_v`) and bitmap blitter (`draw_image`).
- **Typography Subsystem & UI Icon Glyphs (`kernel/graphics/font.*`):**
  - Scalable 8x8 bitmap glyph matrix with integer scaling factors (1x, 2x, 3x).
  - String width and height metrics computation and string alignment helpers (`font_draw_string_centered`).
  - Custom UI icon glyphs (`0x80..0x8A`): minimize (`_`), maximize (`[]`), close (`X`), folder, file, terminal prompt (`>_`), settings gear, CPU, disk, network, and checkmark.
- **Unified Mouse Cursor Overlay (`kernel/graphics/cursor.*`):**
  - Custom 16x16 metallic gold arrow pointer with black outer border and white highlight.
  - 256-pixel background preservation buffer (`saved_bg`) automatically restores the background underneath before rendering at new coordinates, guaranteeing flicker-free cursor movement.
  - 9-bit signed two's complement delta integration from PS/2 mouse IRQ 12 clamped to framebuffer boundaries.
  - Button state tracking for left, right, and middle clicks.
- **Overlapping Window Manager (`kernel/desktop/window.*`):**
  - Full window lifecycle: creation, destruction, movement, resizing, minimize, maximize, restore, and close.
  - Multi-window stacking order (z-order) with automatic bring-to-front and focus management.
  - Precision hit-testing (`wm_hit_test`): Titlebar drag bar, Minimize button, Maximize button, Close button, Client area, and Border.
  - Interactive window dragging with mouse position offset calculation.
  - Dedicated client area buffers (`content`) and paint callback dispatch (`on_paint`, `on_mouse`, `on_key`, `on_close`).
- **Native Scene Compositor (`kernel/desktop/compositor.*`):**
  - 7-layer scene composition pipeline: Desktop background -> Window drop shadows -> Window borders -> Window titlebars -> Window client areas -> System Taskbar -> Application Launcher popup menu & Mouse cursor overlay.
  - Geometric graphite lattice wallpaper with central ZERO OS emblem.
  - Tear-free page flipping with damage region tracking (`fb_flip`).
- **Graphical Login Screen (`kernel/desktop/login.*`):**
  - Branded login card with metallic gold border and header.
  - Username input field (pre-filled with `zero`) and password input field (masked with `*` bullets).
  - Honest development authentication mode: explicit on-screen security banner stating dev mode.
  - Interactive keyboard and mouse login trigger transitioning smoothly into the Desktop Shell.
- **Desktop Shell & Global Shortcuts (`kernel/desktop/shell.*`):**
  - Manages desktop session state (`SHELL_STATE_LOGIN` -> `SHELL_STATE_DESKTOP` -> `SHELL_STATE_LOCKED`).
  - Global keyboard shortcuts: Super/Win key toggles launcher menu, Alt+Tab / F6 cycles active window focus, Alt+F4 closes active window.
  - Bottom taskbar (height 36px) with "ZERO" Start button, open window tabs with active gold indicator, system tray (network, disk, CPU, clock).
- **Native Applications & Utilities (`kernel/desktop/`):**
  - **ZERO Terminal (`terminal.*`):** Interactive console with scrollback buffer, command prompt (`zero@os$ `), and 14 built-in commands: `help`, `clear`, `version`, `memory`, `cpu`, `devices`, `storage`, `network`, `processes`, `uptime`, `ls`, `cd`, `pwd`, `cat`.
  - **ZERO Files (`file_manager.*`):** Native graphical file manager directly connected to VFS and ZEROFS: path navigation, directory listing, directory creation (`vfs_mkdir`), file creation (`vfs_create`), file deletion (`vfs_unlink`), and file preview pane.
  - **ZERO Control Center (`control_center.*`):** Native hardware settings window with 6 interactive tabs: Display, Input, System, Network, Storage, and Desktop Themes.
  - **ZERO System Monitor (`sysmon.*`):** Real-time resource monitor with visual CPU utilization bar, RAM allocation bar, active process table, and storage/network throughput counters.
  - **About ZERO OS (`about.*`):** Official system dialog presenting OS version 5.0.0, architecture specifications, and project identity.
  - **Application Registry (`app.*`):** Standard application framework managing metadata, icon glyphs, and single-instance launching.
- **Verification & Diagnostic Reporting:**
  - In-kernel self-tests (`kernel/debug/selftest_desktop.*`): 8 test suites (100% pass).
  - Automated Python test suite (`tests/test_desktop.py`): 9 test suites (100% pass).
  - Structured desktop diagnostic report (`desktop_report_generate`).

## 4.0.0 — Storage & Networking (COMPLETE: Storage Architecture, Partition Parsers, Block Buffer Cache, ZEROFS Native Filesystem, Network Buffers, Intel E1000 Driver, Network Protocols, BSD Socket API)

### Added (IMPLEMENTED, TESTED)
- **Central Storage Subsystem (`kernel/storage/storage.*`):**
  - Storage coordinator managing physical and virtual block devices with central traffic tracking (`storage_stats_t`).
  - Integer overflow prevention and sector boundary enforcement on all I/O calls.
  - Automated partition scanning on all registered block devices.
- **Partition Table Discovery & Parsers (`kernel/storage/partition.*`):**
  - Master Boot Record (MBR) parser: sector 0 validation (`0xAA55`), 4 primary entries, CHS/LBA translation, bootable checks.
  - GUID Partition Table (GPT) parser: LBA 1 header verification (`EFI PART`), standard IEEE 802.3 CRC32 checksumming, 128-byte partition entry decoding, UTF-16LE partition label conversion.
  - Automated logical partition device registration (`devp1`, `devp2`) wrapping parent devices with LBA offsets.
- **Block Buffer Cache (`kernel/storage/bcache.*`):**
  - 64-entry 4 KiB LRU block cache buffering filesystem metadata and data.
  - Write-back and write-through cache policies with per-block dirty tracking.
  - Synchronous persistence flush via `bcache_sync(dev)` and `bcache_flush_all()`.
- **ZEROFS Native Persistent Filesystem (`kernel/fs/zerofs.*`):**
  - Robust on-disk filesystem layout: Superblock (magic `0x5A45524F`), block and inode allocation bitmaps, 128-byte inodes, 64-byte directory entries.
  - 12 direct block pointers (48 KiB) and single indirect block pointer (4 MiB).
  - VFS integration: `zerofs_format()`, `zerofs_mount()`, path lookup, file creation, multi-block file reading/writing, nested directory creation, and file unlinking.
- **Network Buffer Subsystem (`kernel/net/net_buf.*`):**
  - Reference-counted `net_buf_t` packet buffers with 128-byte headroom for zero-copy protocol header encapsulation (`net_buf_push`, `net_buf_pull`, `net_buf_put`).
- **Network Interface Manager (`kernel/net/net.*`):**
  - Interface abstraction (`net_if_t`) tracking MAC address, IPv4 address, subnet mask, default gateway, DNS server, MTU, and TX/RX counters.
  - Big-endian byte order conversion helpers (`htons`, `ntohs`, `htonl`, `ntohl`).
- **Intel 82540EM (E1000) Gigabit NIC Driver (`kernel/drivers/net/e1000.*`):**
  - PCI bus device discovery (`0x8086:0x100E`, `0x1004`, `0x100F`).
  - MMIO register mapping via HHDM, MAC address readout from RAL/RAH registers.
  - 32-entry Transmit (TX) and Receive (RX) circular DMA rings.
  - Link up detection and fallback synthetic interface when booted without physical NIC.
- **Ethernet II Protocol Layer (`kernel/net/ethernet.*`):**
  - Standard 802.3 framing (14-byte header) and EtherType dispatch (`0x0800` IPv4, `0x0806` ARP).
- **Address Resolution Protocol (`kernel/net/arp.*`):**
  - RFC 826 ARP request broadcasting, reply generation, and 32-entry dynamic ARP cache.
- **Internet Protocol Version 4 (`kernel/net/ipv4.*`):**
  - RFC 791 packet parsing and RFC 791 16-bit Internet checksum calculation and verification.
  - Direct subnet routing and default gateway routing.
- **Internet Control Message Protocol (`kernel/net/icmp.*`):**
  - RFC 792 Echo Request (ping) responder and Echo Request generator.
- **User Datagram Protocol (`kernel/net/udp.*`):**
  - RFC 768 datagram framing and port-based demultiplexing to sockets and DHCP/DNS services.
- **Dynamic Host Configuration Protocol (`kernel/net/dhcp.*`):**
  - RFC 2131 client state machine: `INIT` -> `DISCOVERING` -> `REQUESTING` -> `BOUND`.
  - Parses Subnet Mask (Option 1), Gateway (Option 3), and DNS Server (Option 6).
- **Domain Name System Resolver (`kernel/net/dns.*`):**
  - RFC 1035 query building with QNAME label encoding, and A-record answer parsing.
- **Transmission Control Protocol (`kernel/net/tcp.*`):**
  - RFC 793 connection state machine: `CLOSED`, `LISTEN`, `SYN_SENT`, `SYN_RECEIVED`, `ESTABLISHED`, `FIN_WAIT_1`, `FIN_WAIT_2`, `CLOSE_WAIT`, `CLOSING`, `LAST_ACK`, `TIME_WAIT`.
  - 3-way handshake, sequence/acknowledgement numbers, and sliding window receive buffers.
- **Kernel Socket API & System Calls (`kernel/net/socket.*`, `kernel/syscall/syscall.*`):**
  - BSD socket descriptors (`AF_INET`, `SOCK_STREAM`, `SOCK_DGRAM`, `SOCK_RAW`).
  - Fast system call bindings: `SYS_SOCKET` (11), `SYS_BIND` (12), `SYS_LISTEN` (13), `SYS_ACCEPT` (14), `SYS_CONNECT` (15), `SYS_SEND` (16), `SYS_RECV` (17).
- **Network Diagnostic Reporting (`kernel/net/net_report.*`):**
  - Formatted boot diagnostic report (`ZERO OS NETWORK REPORT`).
- **In-Kernel Self-Tests (`kernel/debug/`):**
  - `selftest_storage.c`: MBR, GPT CRC32, and block cache read/write-back/sync.
  - `selftest_zerofs.c`: ZEROFS formatting, mounting, multi-block I/O, directory creation, unlinking.
  - `selftest_net.c`: net_buf, Ethernet, ARP, IPv4 checksum, ICMP, DHCP, DNS, TCP, sockets.
- **Automated Standalone Test Suite (`tests/test_storage_net.py`):**
  - 13 comprehensive test suites validating all binary layouts, checksums, and state transitions.

## 3.0.0 — Hardware Foundation (COMPLETE: Driver Manager, ACPI Discovery, PCI Enumeration, PS/2 Input, Block Storage, RAM Disk, VirtIO Block, USB Architecture, Hardware Diagnostics)

### Added (IMPLEMENTED, TESTED)
- **Driver and Device Abstraction Layer (`kernel/drivers/driver.*`):**
  - Unified `device_t` and `driver_t` models supporting standard device types (`PCI`, `INPUT`, `BLOCK`, `DISPLAY`, `TIMER`, `NETWORK`, `USB`, `SYSTEM`).
  - Device lifecycle management (`UNINITIALIZED`, `ACTIVE`, `FAILED`, `DISABLED`) and resource tracking (I/O ports, MMIO, IRQs).
  - Centralized thread-safe device registry with registration, name/type lookup, and formatted inventory reporting (`device_dump_inventory`).
- **ACPI Discovery and Table Parsing (`kernel/hardware/acpi.*`):**
  - RSDP discovery supporting Limine bootloader response and fallback memory scanning (EBDA `0x9FC00` and BIOS ROM `0xE0000`).
  - Strict 8-bit modular checksum validation for all ACPI tables.
  - Full support for 64-bit XSDT and 32-bit RSDT pointers.
  - MADT ("APIC") parser extracting Local APIC physical address, Processor Local APIC IDs, I/O APIC MMIO addresses and GSI bases, and Interrupt Source Overrides.
  - HPET ("HPET") parser extracting MMIO base address, sequence number, and minimum clock tick.
- **PCI / PCIe Discovery and Bus Enumeration (`kernel/hardware/pci.*`):**
  - Type 1 PCI Configuration Space access via I/O ports `0xCF8` (address) and `0xCFC` (data).
  - Full bus scan (buses 0..255, slots 0..31, functions 0..7) with multifunction host controller handling.
  - Safe, non-destructive BAR size probing restoring original register values.
  - Extraction of Vendor ID, Device ID, Class code, Subclass, ProgIF, Revision, and IRQ line/pin.
  - Class name translation for display, storage, network, bridges, and serial controllers.
- **Unified Input Subsystem (`kernel/drivers/input.*`):**
  - Thread-safe circular FIFO event queue (`INPUT_QUEUE_SIZE = 128`) with non-blocking polling (`input_poll_event`).
  - Virtual keycodes (`KEY_A`..`KEY_Z`, `KEY_0`..`KEY_9`, Enter, Backspace, Tab, Space, Esc, Arrows, F1..F12).
  - Dynamic modifier tracking (`Shift`, `Ctrl`, `Alt`, `CapsLock`).
  - Virtual keycode to ASCII character translation (`input_keycode_to_ascii`).
- **PS/2 Keyboard Driver (`kernel/drivers/ps2_kbd.*`):**
  - Port `0x60` (data) and `0x64` (status/command) controller initialization and buffer flushing.
  - Hardware interrupt handler on IRQ 1 (vector `0x21`) with EOI signaling.
  - Scancode Set 1 make and break code translation, extended `0xE0` key handling, and input event dispatch.
- **PS/2 Mouse Driver (`kernel/drivers/ps2_mouse.*`):**
  - Auxiliary device initialization, configuration byte setup, and data reporting activation (`0xF4`).
  - Hardware interrupt handler on IRQ 12 (vector `0x2C`) with EOI signaling.
  - 3-byte packet state machine with Bit 3 sanity check, 9-bit signed X/Y relative deltas, and left/right/middle button transitions.
- **Generic Block Storage Abstraction (`kernel/drivers/block.*`):**
  - Standard block device interface (`block_device_t`) with sector-granular operations (`read_blocks`, `write_blocks`, `flush`).
  - Strict parameter validation, 64-bit LBA arithmetic overflow checking (`chk_add_u64`), boundary enforcement, and read-only protection.
- **RAM Disk Block Storage (`kernel/drivers/ramdisk.*`):**
  - In-memory RAM disk (`ramdisk0`) supporting configurable sector capacity (default 2048 sectors = 1 MiB) for safe testing.
- **VirtIO Block Storage Driver (`kernel/drivers/virtio_blk.*`):**
  - QEMU VirtIO block PCI device discovery (Vendor `0x1AF4`, Device `0x1001`/`0x1042`), bus mastering activation, and legacy I/O control.
  - Physically contiguous vring allocation (Descriptor table, Available ring, Used ring).
  - Block request header dispatch and synchronous polling with timeout protection.
  - Registered block device `"vda"` with capacity reporting.
- **USB Subsystem Architecture (`kernel/drivers/usb.*`):**
  - Architectural models for host controllers: UHCI, OHCI, EHCI, xHCI.
  - Standard USB packed descriptor structures (`usb_setup_packet_t`, `usb_device_descriptor_t`, `usb_config_descriptor_t`, `usb_interface_descriptor_t`, `usb_endpoint_descriptor_t`).
  - PCI discovery scanning for USB controllers (Class `0x0C`, Subclass `0x03`).
- **Hardware Diagnostic Reporting (`kernel/hardware/hw_report.*`):**
  - Comprehensive, formatted diagnostic report querying real CPUID (Vendor, Brand string, Features), PMM physical memory stats, ACPI/MADT tables, PCI devices, input devices, block storage devices, and GOP framebuffer display information.
- **Milestone 3.0.0 Hardware Self-Tests & Test Suite:**
  - `selftest_acpi.c`: Checksum validation, MADT records, and HPET discovery.
  - `selftest_pci.c`: PCI config space, bus enumeration, and safe BAR probing.
  - `selftest_input.c`: Event queue FIFO order, scancode translation, modifiers, and mouse packets.
  - `selftest_block.c`: Sector read/write parity, multi-block I/O, bounds rejection, and read-only protection.
  - `selftest_usb.c`: Binary descriptor packing, setup packets, and controller models.
  - Standalone automated test suite `tests/test_hardware.py` passing all 7 test categories (100% OK).

## 2.0.0 — System Architecture (COMPLETE: Processes, Threads, Preemptive Scheduler, VFS, RAMFS, IPC, Syscalls, ELF64 Loader, Ring 3 Transition)

### Added (IMPLEMENTED, TESTED)
- **Process Management Subsystem:**
  - Process Control Block (PCB) defining process lifecycle states (`NEW`, `READY`, `RUNNING`, `BLOCKED`, `TERMINATED`, `ZOMBIE`), PID allocation, and parent-child hierarchy (`kernel/process/process.*`).
  - Private Virtual Address Spaces: Each process owns an isolated PML4 table cloned from the kernel higher-half mappings.
  - Resource accounting: open file descriptor tables (`fd_table`), memory bytes allocated, CPU execution ticks, and automated resource cleanup upon exit (`process_cleanup`).
- **Thread Management Subsystem:**
  - Thread Control Block (TCB) with dedicated 16 KiB kernel execution stacks, Ring 3 user stacks, scheduling states, time slice allocation, and callee-saved register context structures (`kernel/process/thread.*`).
  - Separate kernel and user thread constructors (`thread_create_kernel`, `thread_create_user`).
- **Preemptive Round-Robin Scheduler:**
  - Preemptive time-sliced scheduler driven by APIC timer ticks (100 Hz, default 50 ms time slice).
  - Ready run queue, sleeping thread queue with deadline expiration, and dedicated system idle thread (`kernel/process/scheduler.*`).
  - Assembly context switch routine (`context_switch`) saving/restoring callee-saved registers (`rbx, rbp, r12, r13, r14, r15, rsp`), switching stacks, and automatically updating `CR3` on process boundary crossings (`kernel/process/context.asm`).
- **Ring 3 User Mode Transitions & GDT Expansion:**
  - Configured GDT user descriptors for User Data (`0x28`, RPL 3) and User Code (`0x30`, RPL 3) with dynamic TSS `rsp0` kernel stack updating via `gdt_set_kernel_stack()` (`kernel/arch/x86_64/gdt.c`).
  - `enter_user_mode` assembly routine using `iretq` with user data selector, user stack pointer, RFLAGS (IF enabled), user code selector, and entry RIP.
- **Fast System Call Interface (x86_64 syscall/sysret):**
  - Configured MSRs: `MSR_STAR` (0xC0000081), `MSR_LSTAR` (0xC0000082), `MSR_SFMASK` (0xC0000084), and `EFER.SCE` (`kernel/syscall/syscall.c`).
  - Assembly entry stub `syscall_entry` saving user context, switching to process kernel stack, and dispatching to C handlers (`kernel/syscall/syscall_entry.asm`).
  - Security validation: `syscall_validate_user_ptr()` enforcing 48-bit canonical user lower-half boundaries (`< 0x0000800000000000`) and preventing arithmetic pointer wraparound.
  - Implemented syscalls: `SYS_EXIT` (1), `SYS_YIELD` (2), `SYS_SLEEP` (3), `SYS_GETPID` (4), `SYS_WRITE` (5), `SYS_READ` (6), `SYS_OPEN` (7), `SYS_CLOSE` (8), `SYS_IPC_SEND` (9), `SYS_IPC_RECV` (10).
- **Virtual File System (VFS):**
  - Uniform abstraction layer with `vnode_t`, `file_t`, mount tables, and operations (`open`, `close`, `read`, `write`, `seek`, `lookup`, `create`, `mkdir`, `unlink`) in `kernel/fs/vfs.*`.
- **In-Memory RAMFS Filesystem:**
  - Root in-memory filesystem mounted at `/` supporting hierarchical directory trees, dynamic file resizing via `krealloc`, seekable read/write streams, and safe unlink operations (`kernel/fs/ramfs.*`).
- **Inter-Process Communication (IPC):**
  - Spinlock-protected ring-buffered mailboxes per process, unicast message delivery, strict payload bounds (`IPC_MAX_PAYLOAD`), mailbox capacity enforcement (`IPC_QUEUE_CAP`), and automated process cleanup (`kernel/ipc/`).
- **ELF64 Executable Loader:**
  - Complete ELF64 header validation (`0x7F 'E' 'L' 'F'`, 64-bit, little-endian, `EM_X86_64`, `ET_EXEC`/`ET_DYN`).
  - `PT_LOAD` segment parsing, memory allocation via PMM, page mapping into process PML4 with $W \oplus X$ permissions, file data copying, BSS zeroing, and user stack mapping at `0x00007FFFF0000000` (`kernel/loader/`).

## 1.0.0 — Core Foundation (COMPLETE: PMM, VMM, Heap, Spinlocks, Local APIC and Timer validated)
...
## 0.0.0 — Genesis
...
