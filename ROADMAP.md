# Roadmap

Milestones 0.0.0 (Genesis), 1.0.0 (Core Foundation), 2.0.0 (System Architecture), 3.0.0 (Hardware Foundation), 4.0.0 (Storage & Networking), 5.0.0 (ZERO Desktop), 6.0.0 (ZERO Ecosystem), 7.0.0 (ZERO Security), 8.0.0 (ZERO Intelligence), and 9.0.0 (ZERO Ultimate) are complete, integrated, and validated. All ten official roadmap milestones are fulfilled (`VERSION_POLICY.md`). No official Milestone 10.0.0 exists.

| Version | Name | Scope and Deliverables | Status | Depends on |
|---------|------|---------------------------------------------------|--------|------------|
| 0.0.0 | Genesis | Boot via UEFI+Limine, own kernel entry, serial + framebuffer log, GDT/TSS, IDT, exceptions (0-31), panic, QEMU tests | Complete | — |
| 1.0.0 | Core Foundation | Physical memory manager (PMM), 4-level paging (VMM), Kernel Heap (8 size classes + large pages), Atomic Spinlocks & IRQ-save sections, Local APIC, APIC Timer (100 Hz) | Complete | 0.0.0 |
| 2.0.0 | System Architecture | Processes (PCB), threads, preemptive round-robin scheduler, IPC, VFS + RAMFS, fast system calls (MSR STAR/LSTAR/SFMASK), ELF64 loader, Ring 3 isolation | Complete | 1.0.0 |
| 3.0.0 | Hardware Foundation | Driver manager, ACPI (RSDP/MADT/HPET), PCI/PCIe enumeration, PS/2 Keyboard & Mouse, unified input queue, generic block devices, RAM disk, VirtIO block, USB architecture | Complete | 2.0.0 |
| 4.0.0 | Storage & Networking | Storage coordinator, MBR/GPT parsers, 4 KiB block cache (bcache), ZEROFS native persistent filesystem, net_buf zero-copy buffers, Intel 82540EM (E1000) driver, full TCP/IP stack (ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP), BSD socket API syscalls (11-17) | Complete | 3.0.0 |
| 5.0.0 | ZERO Desktop | Tear-free double-buffered linear framebuffer, damage tracking, 2D vector primitives, scalable font engine & UI glyphs, flicker-free cursor overlay, overlapping Window Manager, 7-layer scene Compositor, Desktop Shell, Graphical Login, and native apps (Terminal, Files, Control Center, Sysmon, About) | Complete | 4.0.0 |
| 6.0.0 | ZERO Ecosystem | .zapp Package format, CRC32/SHA256 integrity, traversal filter, ZERO Package Manager (zpm), ZERO Store graphical center, native apps (Calc, Editor, Viewer, Archiver, Netmon), persistent /data/config, toast notifications, ZERO SDK (crt0, linker_user.ld, headers, hello world) | Complete | 5.0.0 |
| 7.0.0 | ZERO Security | Central Security Architecture, capability access control (unforgeable tokens, attenuation, revocation), hierarchical process sandboxing (RESTRICTED/BASIC/STD/SYSTEM), filesystem DAC (0777 mode bits, UID/GID), freestanding FIPS SHA-256 + CSPRNG, package digest verification, packet-filtering firewall (32 prioritized rules, default-deny), audit logging (/data/log/audit.log), ZERO Security Center (7 views) | Complete | 6.0.0 |
| 8.0.0 | ZERO Intelligence | ZERO AI assistant daemon, pluggable provider abstraction (Local CPU, Host Adapter, Remote), local model manager (256MB memory budget, single-active-model policy), permission-gated system automation broker, interactive User Approval Gate, multi-turn bounded context engine, persistent conversational memory (/data/ai), security audit bridge, native 6-view AI Assistant application, Developer C SDK | Complete | 7.0.0 |
| **9.0.0** | **ZERO Ultimate** | **ZERO Gaming Mode (NORMAL/GAMING/POWER_SAVER), low-latency scheduler profile, graphics device abstraction (Intel HD 500/VirtIO/Bochs), SIMD blitter (1597 FPS), audio subsystem (PCM stereo, null sink), native demo game ("ZERO Strike"), Gaming Control Center GUI, AI resource coordination, performance benchmarks, final hardening** | **Complete** | 8.0.0 |

Security is *progressive*: privilege separation and memory protection start in 1.0.0 and are extended in each milestone; the 7.0.0 / 8.0.0 / 9.0.0 labels are where they reach full integration, not where they begin.

## Roadmap Status

All ten official milestones in the original ZERO OS roadmap are complete. Development has concluded successfully.
