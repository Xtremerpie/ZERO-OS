# ZERO OS — Hardware Compatibility Matrix

**Target Platform:** Intel Celeron J3355 Workstation / Reference Target  
**Milestone:** 3.0.0 Hardware Foundation  

---

## 1. Hardware Specification & Status

| Subsystem | Target Hardware | Supported in ZERO OS 3.0.0 | Status / Notes |
|---|---|---|---|
| **CPU** | Intel Celeron J3355 @ 2.00 GHz (2C/2T) | **Yes** | 64-bit Long Mode, CPUID identification, SSE/SSE2, Local APIC |
| **Firmware** | UEFI 64-bit (Limine 11.4.1) | **Yes** | Boot protocol rev 6, GOP 32-bpp linear RGB framebuffer |
| **Memory** | 4 GB LPDDR4/DDR3L | **Yes** | PMM bitmap frame allocator, sub-1 MiB exclusion, VMM 4-level paging |
| **ACPI** | ACPI 2.0+ / 1.0 Tables | **Yes** | RSDP discovery, RSDT/XSDT, MADT parser (CPUs, IOAPIC), HPET |
| **PCI / PCIe** | Intel Apollo Lake PCI Root Complex | **Yes** | Type 1 configuration space, bus 0..255 scan, non-destructive BAR probing |
| **Graphics** | Intel HD Graphics 500 | **Yes** | Linear GOP framebuffer console (32 bpp, ZERO OS palette) |
| **Keyboard** | Standard PS/2 Keyboard | **Yes** | IRQ 1 handler, Scancode Set 1 make/break translation, modifiers |
| **Mouse** | Standard PS/2 Mouse | **Yes** | IRQ 12 handler, 3-byte packet decoding, 9-bit relative deltas, buttons |
| **Block Storage** | SATA AHCI / SSD / VirtIO | **Yes** | Generic block abstraction, 512B sectors, RAM disk (`ramdisk0`), VirtIO (`vda`) |
| **USB** | Intel xHCI Controller (0x0C:0x03) | **Architectural** | Host controller discovery, standard packed descriptor models (Setup, Device, Config, Interface, EP) |
| **Audio** | Intel HD Audio | *Later (4.0.0+)* | Deferred |
| **Networking** | Realtek Gigabit Ethernet / Intel WiFi | *Later (4.0.0)* | Milestone 4.0.0 Storage & Networking |

---

## 2. Emulation vs Bare-Metal Support

- **QEMU Machine (`-M q35` or standard PC):** Fully verified with OVMF UEFI firmware, VirtIO block storage, PS/2 HID devices, ACPI tables, and Local APIC timer.
- **Physical Machine (Intel Celeron J3355):** Ready for boot preparation. USB boot requires generating a bootable FAT32/GPT flash drive with Limine EFI binaries and `zero-kernel.elf`.
