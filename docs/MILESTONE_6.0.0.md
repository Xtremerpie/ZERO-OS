# ZERO OS 6.0.0 — ZERO Ecosystem Milestone Report

**Codename:** ZERO Ecosystem (The Native Application Revolution)  
**Architecture:** x86_64 Higher-Half Microkernel  
**Boot Protocol:** UEFI + Limine Bootloader  
**User Interface:** Native ZERO Desktop Compositor  
**Status:** COMPLETE (Milestone 6.0.0)

---

## 1. Executive Summary

ZERO OS 6.0.0 inaugurates the native application platform and software ecosystem of ZERO OS. Moving beyond core kernel mechanisms, hardware drivers, persistent storage, networking stacks, and desktop graphics, Milestone 6.0.0 establishes an independent, sovereign software distribution, management, and runtime environment.

Key pillars delivered in this milestone:
1. **ZERO Application Package Format (`.zapp`)**: An integrity-verified, binary package container featuring 128-byte headers, 384-byte manifests, 144-byte file descriptors, CRC-32 and SHA-256 cryptographic verification, and strict path traversal safety guards.
2. **ZERO Package Manager (`zpm`)**: A transactional kernel subsystem and unified CLI offering installation, uninstallation, inventory listing, verification, and remote repository queries.
3. **ZERO Software Center (`ZERO Store`)**: A full-featured native graphical app store integrated into ZERO Desktop with category filtering, real-time search, package cards, and one-click install/launch workflows.
4. **Native Application Suite**: Six brand-new desktop applications (ZERO Calculator, ZERO Text Editor, ZERO Image Viewer, ZERO Archive Viewer, ZERO Network Monitor, and ZERO Store) plus major upgrades to ZERO Terminal, ZERO Files, and ZERO Control Center.
5. **Application Configuration Subsystem (`/data/config/<app_id>.cfg`)**: Persistent, typed key-value preferences engine.
6. **Desktop Toast Notifications Overlay**: Real-time non-blocking notification toasts composited over the desktop above the taskbar with animated countdown dismissal.
7. **ZERO Software Development Kit (SDK)**: Native C11 headers, Ring 3 x86_64 syscall wrappers, user-space linker script (`0x400000`), assembly startup code (`crt0.asm`), host packaging utility (`tools/zapp_tool.py`), and a canonical Hello World reference application.

ZERO OS has zero dependence on Linux/glibc, Windows/Win32, or web browser runtimes. The entire ecosystem is built from scratch.

---

## 2. Architecture & Subsystems Delivered

### 2.1 The `.zapp` Application Package Format (`kernel/apps/zapp.h`, `zapp.c`)
- **128-Byte Binary Header (`zapp_header_t`)**:
  - Magic signature `0x5A415050` (`"ZAPP"` in little-endian).
  - Format version `1`, flags, semantic version string (`1.0.0`), application ID (`zero.calc`), target architecture (`x86_64`).
  - Offsets and lengths for manifest, file entry table, and compressed/raw payload data.
  - IEEE 802.3 CRC-32 payload checksum and 256-bit SHA-256 cryptographic digest.
- **384-Byte Manifest (`zapp_manifest_t`)**:
  - Application display name, description, category, author, license identifier.
  - Relative entry point binary path (`bin/calc`), relative icon asset path (`icons/calc.png`).
  - Required permission bitmask (`ZAPP_PERM_WINDOW`, `STORAGE`, `NETWORK`, `DEVICES`, `AUDIO`).
  - Minimum ZERO OS kernel version requirement (`0x060000` = 6.0.0).
- **144-Byte File Table Entry (`zapp_file_entry_t`)**:
  - Path inside container (up to 128 characters), relative payload offset, byte length, file permissions (`0755`/`0644`), and entry flags (`EXEC`, `ICON`, `CONFIG`, `RESOURCE`).
- **Path Traversal Security Defense**:
  - Rejects leading slashes, backward slashes, drive colons, and relative parent segment references (`..`). Seven attack vectors actively blocked.

### 2.2 ZERO Package Manager (`kernel/apps/zpm.h`, `zpm.c`)
- **Persistent Database**: Tracks installed applications at `/data/pkg/installed.db` and installs binaries into isolated application sandboxes under `/data/apps/<app_id>/`.
- **Atomic Operations**: Guarantees all-or-nothing package extraction and transactional rollbacks on failure.
- **Unified CLI (`zpm_exec_cli`)**:
  - `zpm list`: Formatted table of installed packages and versions.
  - `zpm info <app_id>`: Detailed inspection of manifest, capabilities, and dependencies.
  - `zpm install <path>`: Unpacks `.zapp` file from VFS into root filesystem.
  - `zpm uninstall <app_id>`: Removes files and cleans registry.
  - `zpm search <query>`: Queries local and remote package repositories.
  - `zpm verify <app_id>`: Audits disk files against registered package hashes.
  - `zpm update`: Synchronizes local package catalog with repository index.

### 2.3 ZERO Software Center / ZERO Store (`kernel/desktop/store.h`, `store.c`)
- Native 620x420 resizable window running within ZERO Desktop.
- 6 Category views: *All Apps*, *Productivity*, *Development*, *Utilities*, *Internet*, *Media*, and *Installed*.
- Real-time search query filtering over package names and categories.
- Visual package cards featuring application titles, versions, categories, download sizes, and dynamic state-aware action buttons (*Install*, *Launch*, *Uninstall*).
- Transactional status bar displaying installation progress and ZPM feedback.

### 2.4 Native Applications Suite
1. **ZERO Calculator (`kernel/desktop/calc.h`, `calc.c`)**:
   - 280x360 window, dark void display, metallic gold operation accents.
   - Interactive 4x5 keypad grid with mouse and keyboard input.
   - Arithmetic operations (+, -, *, /), sign flipping (+/-), clear (C/CE), percentage calculations, and overflow protection.
2. **ZERO Text Editor (`kernel/desktop/editor.h`, `editor.c`)**:
   - 560x380 window with multi-line text buffer (20 lines x 64 columns).
   - Real-time cursor navigation, line wrap, backspace, enter, and text entry.
   - Direct VFS file persistence: New file, Open file, Save file.
3. **ZERO Image Viewer (`kernel/desktop/viewer.h`, `viewer.c`)**:
   - 500x340 window supporting SMPTE color bars, geometric alignment crosshairs, and icon glyph matrix.
   - Zoom scaling engine (1x, 2x, 3x, 4x) and viewport rendering.
4. **ZERO Archive Viewer (`kernel/desktop/archiver.h`, `archiver.c`)**:
   - 540x360 package inspector displaying header metadata, manifest permissions, and individual file entries.
   - One-click package extraction directly wired to ZPM.
5. **ZERO Network Monitor (`kernel/desktop/netmon.h`, `netmon.c`)**:
   - 540x360 telemetry console with 3 interactive tabs (*Overview*, *ARP Cache*, *Sockets*).
   - Real-time TX/RX packet and byte throughput counters, error rates, interface parameters (`eth0`, MAC, IP, Subnet, Gateway, MTU).
   - Dynamic ARP table viewer and BSD socket descriptor inspector.
6. **Desktop Application Registry & Upgrades**:
   - Registry updated to 11 native applications (`kernel/desktop/app.c`).
   - ZERO Terminal updated with native `zpm` CLI commands.
   - ZERO Files updated with automated file associations (`.zapp` -> archiver, `.txt`/`.cfg` -> editor, `.bmp` -> viewer).
   - ZERO Control Center updated with a dedicated "Permissions" security auditing tab.
   - About ZERO OS updated with official 6.0.0 specs.

### 2.5 Application Configuration Framework (`kernel/apps/config.h`, `config.c`)
- Key-value configuration parser operating over `/data/config/<app_id>.cfg`.
- Typed accessors: `config_get_string`, `config_get_int`, `config_get_bool`, `config_set_string`, `config_set_int`, `config_set_bool`.
- Handles blank lines, comments (`#`), whitespace trimming, and file persistence.

### 2.6 Desktop Notification System (`kernel/desktop/notification.h`, `notification.c`)
- Lightweight toast notifications (240x52px) rendered above the taskbar in the bottom-right corner.
- Accent gold indicator bar, custom icon glyph, title, message text, and countdown timer bar.
- Fully integrated into compositor pipeline and shell tick loop.

### 2.7 ZERO Software Development Kit (SDK) & Build Tools
- Standard header suite in `sdk/include/zero/`:
  - `syscall.h`: Direct Ring 3 syscall wrappers (`zero_exit`, `zero_write`, `zero_read`, `zero_open`, `zero_close`, `zero_yield`).
  - `app.h`: Application configuration structure and `ZERO_APP_MAIN` macro.
  - `window.h`: Window creation, event loop, and lifecycle callbacks.
  - `draw.h`: Drawing surface primitives and ZERO palette constants.
  - `event.h`: Keyboard, mouse, and system event structures.
- User-mode linker script (`sdk/linker_user.ld`) with entry at `0x400000`.
- x86_64 CRT0 assembly startup file (`sdk/crt0.asm`).
- Canonical sample project: `sdk/templates/hello_world/main.c`.
- Host packaging tool: `tools/zapp_tool.py` (create, validate, inspect, and extract `.zapp` archives).

---

## 3. Verification & Test Results

| Test Suite | Components Tested | Result |
| :--- | :--- | :--- |
| `tests/check_versions.py` | 10 official milestone versions manifest & version.h | **PASSED (100%)** |
| `tests/test_hardware.py` | ACPI, MADT, PCI BARs, PS/2, Block, USB | **PASSED (100%)** |
| `tests/test_storage_net.py` | MBR, GPT, ZEROFS, Ethernet, IP, TCP/UDP, Sockets | **PASSED (100%)** |
| `tests/test_desktop.py` | Colors, Clipping, Fonts, Cursor, WM, Shell, Apps | **PASSED (100%)** |
| `tests/test_ecosystem.py` | ZAPP Headers, Traversal Defense, Checksums, ZPM, Config, SDK | **PASSED (100%)** |
| `kernel/debug/selftest_ecosystem.c` | In-Kernel Ecosystem Validation (7 sub-suites) | **PASSED (100%)** |

All tests pass 100% with zero regressions.
