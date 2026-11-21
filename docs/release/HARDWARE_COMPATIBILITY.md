# ZERO OS 9.0.0 — Hardware Compatibility List (HCL)
**Milestone:** 9.0.0 — ZERO Ultimate  
**Codename:** `THE FINAL ASCENSION`  
**Architecture:** x86_64 Higher-Half Native Operating System  

---

## 1. Verified Hardware Baseline

ZERO OS was specifically designed, optimized, and verified for the following primary hardware environment:

| Component | Target Specification | Supported / Driver Status |
|---|---|---|
| **CPU** | Intel Celeron J3355 (Apollo Lake, 2 cores / 2 threads @ 2.0 GHz) | **Full Support:** x86_64 long mode, APIC, MSR syscalls, SSE4.2 / AVX2 |
| **Memory** | 4 GB DDR3L/DDR4 (shared with Intel HD Graphics) | **Full Support:** PMM 4 KiB bitmap allocator, 4-level VMM with $W \oplus X$ |
| **Storage** | SATA 3.0 / NVMe SSD, GPT Partition Tables | **Full Support:** ZEROFS native filesystem, GPT parser, 64-entry LRU `bcache` |
| **Graphics** | Intel HD Graphics 500 (Gen9 LP, 12 Execution Units) | **Full Support:** 32-bpp linear framebuffer, 2D SIMD blitter, double buffering |
| **Audio** | Realtek ALC / Intel High Definition Audio (HDA) | **Full Support:** Audio device abstraction (`audio_dev_t`), 16-bit stereo PCM, null sink fallback |
| **Network** | Intel 82540EM / Realtek RTL8111 Gigabit Ethernet | **Full Support:** DMA ring buffers, Ethernet II, ARP, IPv4, ICMP, UDP, TCP RFC 793, BSD sockets |
| **Input** | Standard PS/2 Keyboard & Mouse / USB Emulation | **Full Support:** IRQ 1 / IRQ 12, Scancode Set 1 make/break, 3-byte packets, 9-bit deltas |
| **Firmware** | UEFI 2.x (x86_64) via Limine 11.4.1 Bootloader | **Full Support:** GOP linear framebuffer, RSDP discovery, MADT APIC tables, HPET |

---

## 2. Virtual Machine & Hypervisor Support

| Virtual Machine | Hypervisor / Settings | Driver Support Level |
|---|---|---|
| **QEMU / KVM** | `-M q35`, `-cpu host` or `max`, OVMF UEFI firmware | **Tier 1 (Primary Development):** VirtIO-blk (`vda`), E1000 NIC, Bochs/VirtIO GPU, PS/2 HID |
| **VMware Workstation** | UEFI boot, VMXNET3 or E1000, SATA virtual disk | **Tier 1:** Full support for kernel, GUI desktop, network, and storage |
| **VirtualBox** | EFI enabled, Intel PRO/1000 MT Desktop, ICH9 chipset | **Tier 1:** Full support |
| **Bochs** | x86_64, VBE 3.0, Cirrus/Bochs VGA | **Tier 2:** Supported via generic VBE framebuffer |

---

## 3. Unsupported Hardware & Explicit Limitations

- **Legacy 32-bit (x86 / i386) CPUs:** Strictly not supported. ZERO OS requires 64-bit long mode (`EM_X86_64`).
- **Legacy BIOS (MBR-only firmware):** Strictly not supported. ZERO OS boots exclusively via UEFI and GPT partition tables.
- **Proprietary GPU Microcode Submission (NVIDIA / AMD Radeon 3D):** Milestone 9.0.0 establishes the 2D blitter and capability interface; discrete 3D ring submission engines for proprietary GPUs are deferred to future post-roadmap research. Universal software blitting fallback is guaranteed across all systems.
