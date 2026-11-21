# ZERO OS 9.0.0 — HARDWARE COMPATIBILITY & ABSTRACTION
## Host Platform Reference, Virtualization & Driver Subsystems
### Release: 9.0.0 | Target Hardware Reference: Intel Celeron J3355 (4 GB RAM)

---

### 1. REFERENCE HOST HARDWARE PLATFORM

ZERO OS was architected and sized for low-resource modern x86_64 systems:

| Hardware Component | Reference Specification | Driver Architecture / Mapping |
|---|---|---|
| **CPU** | Intel Celeron J3355 (Apollo Lake) @ 2.00–2.50 GHz | 64-bit Long Mode, Local APIC, MSR management |
| **Cores / Threads** | 2 Cores / 2 Threads | Preemptive round-robin scheduler with low-latency gaming priority |
| **System RAM** | 4 GB LPDDR4 / DDR3L | Bitmap Physical Memory Manager; guest sized to 256 MB |
| **Integrated GPU** | Intel HD Graphics 500 (Gen9, 12 EUs) | PCI Vendor `0x8086`, Device `0x5A85` (UEFI GOP linear framebuffer) |
| **Storage** | SATA/NVMe SSD | VirtIO-Block emulation / AHCI command structures; ZEROFS |
| **Audio** | Intel High Definition Audio | PCI Class `0x04`; circular PCM ring buffer with Software Null Sink |
| **Input Devices** | USB / PS/2 Keyboard & Mouse | Interrupt-driven PS/2 controller emulation (IRQ 1 & IRQ 12) |
| **Firmware** | UEFI 64-bit (GPT partition layout) | Limine bootloader protocol v1 |

---

### 2. SUBSYSTEM CAPABILITY MATRIX

The operating system explicitly distinguishes between hardware abstraction support, software emulation, and unimplemented physical features:

| Subsystem | Driver Module | Implementation Status | Technical Capability |
|---|---|---|---|
| **Paging & MMU** | `kernel/memory/vmm.c` | **NATIVE BARE-METAL** | 4-level paging (`CR3`), 4 KB pages, canonical bounds, W^X enforcement |
| **Interrupt Controller** | `kernel/interrupts/apic.c` | **NATIVE BARE-METAL** | Local APIC discovery via MSR `0x1B`, legacy 8259 PIC masking |
| **Timer** | `kernel/arch/x86_64/timer.c` | **NATIVE BARE-METAL** | Local APIC periodic interrupt at 1,000 Hz (1 ms tick) |
| **PCI Enumeration** | `kernel/hardware/pci.c` | **NATIVE BARE-METAL** | Type 1 configuration space probe (ports `0xCF8`/`0xCFC`), BAR discovery |
| **ACPI Discovery** | `kernel/hardware/acpi.c` | **NATIVE BARE-METAL** | RSDP verification, RSDT/XSDT checksumming, MADT parsing |
| **Storage Driver** | `kernel/drivers/virtio_blk.c` | **VIRTUAL HARDWARE** | VirtIO-Block queue DMA; ZeroFS native on-disk filesystem |
| **Network Driver** | `kernel/drivers/virtio_net.c` | **VIRTUAL HARDWARE** | VirtIO-Net packet ring; Ethernet II, ARP, IPv4, TCP/UDP stack |
| **2D Graphics Blitter** | `kernel/graphics/gpu.c` | **OPTIMIZED SOFTWARE** | 64-bit SIMD memory copy loop over UEFI GOP 32-bit ARGB framebuffer |
| **Hardware 3D GPU** | `kernel/graphics/gpu.c` | **NOT IMPLEMENTED** | No hardware 3D command rings, vertex shaders, or Vulkan/OpenGL drivers |
| **Audio Subsystem** | `kernel/drivers/audio.c` | **SOFTWARE NULL SINK** | 8 KB circular PCM buffer, volume clamping; no physical HDA DMA playback |
| **Desktop Compositor**| `kernel/desktop/compositor.c`| **NATIVE GRAPHICAL** | 1280x800 double-buffered window compositing, z-ordering, hit-testing |
| **Security Architecture**| `kernel/security/` | **NATIVE KERNEL** | Capability tokens, process jails, FIPS SHA-256 KATs, audit logging |
| **AI Subsystem** | `kernel/intelligence/` | **NATIVE KERNEL** | In-kernel micro-engine, permission broker, gaming mode throttling |

---

### 3. VIRTUALIZATION ENGINE COMPATIBILITY

| Hypervisor / Emulator | Compatibility | Recommended Configuration |
|---|---|---|
| **QEMU (Windows / Linux)** | **PRIMARY / VERIFIED** | `-machine q35 -cpu qemu64 -m 256M -accel tcg -drive ...,snapshot=on` |
| **VirtualBox 7.x** | **COMPATIBLE** | Create "Other 64-bit" VM, enable EFI, attach virtual disk with write-through disabled |
| **VMware Workstation** | **COMPATIBLE** | Workstation 17+, EFI firmware enabled, VirtIO or SATA virtual disk |
| **Physical Bare-Metal** | **NOT RECOMMENDED** | Physical disk installer not included; requires manual USB GPT preparation |
