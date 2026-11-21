# ZERO OS 9.0.0 — Troubleshooting & Diagnostic Guide
**Milestone:** 9.0.0 — ZERO Ultimate  
**Codename:** `THE FINAL ASCENSION`  

---

## 1. Diagnostic Reporting Tools

ZERO OS integrates structured diagnostic reporting across all subsystems:

| Report Command / GUI | Source Component | Output / Log Target |
|---|---|---|
| **`hw_report_generate()`** | `kernel/hardware/hw_report.c` | COM1 Serial / Kernel Log (CPU, ACPI, PCI, Block devices) |
| **`net_report_generate()`** | `kernel/net/net_report.c` | Network interface status, MAC, IP, ARP table, active sockets |
| **`desktop_report_generate()`**| `kernel/debug/selftest_desktop.c` | Framebuffer resolution, double-buffering, window manager |
| **`ecosystem_report_generate()`**| `kernel/debug/selftest_ecosystem.c` | ZPM database count, Store catalog, `.zapp` package signatures |
| **`security_report_generate()`** | `kernel/debug/selftest_security.c` | Capability count, active sandbox profiles, firewall rules |
| **`intelligence_report_generate()`**| `kernel/debug/selftest_intelligence.c`| AI daemon health, active model, memory budget, provider |
| **`ultimate_report_generate()`** | `kernel/debug/selftest_ultimate.c` | Gaming profile, low-latency scheduler, GPU & audio status |
| **ZERO Security Center** | `kernel/desktop/security_center.c` | 7-view interactive GUI for capabilities, sandbox, firewall |
| **Gaming Control Center** | `kernel/desktop/gaming_control.c` | 5-view interactive GUI for scheduler, profiles, and performance |
| **Audit Log Stream** | `/data/log/audit.log` | Persistent, non-blocking structured audit trail |

---

## 2. Common Scenarios & Resolutions

### 2.1 Framebuffer Displays Black Screen or Headless Mode
- **Cause:** UEFI GOP (Graphics Output Protocol) linear framebuffer is unavailable or resolution handshake failed.
- **Resolution:** The kernel automatically falls back to headless serial output on `COM1` (115200 8N1). Ensure OVMF firmware is compiled with GOP support (`-vga std` in QEMU).

### 2.2 Network Interface Not Obtaining DHCP Lease
- **Cause:** Virtual NIC not connected to local bridge or DHCP server unresponsive.
- **Resolution:** Check `net_report_generate()` in serial log. Ensure QEMU is launched with `-netdev user,id=net0 -device e1000,netdev=net0`.

### 2.3 Audio Plays Silently
- **Cause:** Physical audio hardware absent or running in generic virtualization mode.
- **Resolution:** The native Audio Subsystem safely defaults to `AUDIO_DEVICE_NULL_SINK` to absorb PCM streams without stalling. To test emulated hardware, add `-device intel-hda -device hda-duplex` to the QEMU launch script.

### 2.4 Game Does Not Auto-Switch to Gaming Mode
- **Cause:** `auto_switch=0` configured in `/data/config/gaming.cfg`.
- **Resolution:** Open the **Gaming Control Center** on ZERO Desktop and select `[GAMING]` profile manually, or edit `/data/config/gaming.cfg` to set `auto_switch=1`.
