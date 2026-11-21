# Milestone 3.0.0 — Hardware Foundation: Engineering Completion Report

**Official Version:** 3.0.0  
**Codename:** Hardware Integration (Hardware Foundation)  
**Status:** IMPLEMENTED, TESTED, AND VALIDATED  
**Architecture:** x86_64 Higher-Half Preemptive Multitasking Kernel  
**Firmware:** UEFI / Limine 11.4.1 (Base Revision 6)  
**Target Hardware:** Intel Celeron J3355, 4 GB RAM, SSD, GPT  

---

## 1. Executive Summary

Milestone 3.0.0 (Hardware Foundation) establishes the device driver model, firmware discovery, bus enumeration, human interface device (HID) input, and block storage infrastructure for ZERO OS. All requirements specified in the engineering directive have been designed, implemented, integrated into the kernel boot sequence, and verified through both in-kernel self-tests and the automated hardware test suite (`tests/test_hardware.py`).

---

## 2. Delivered Hardware Subsystems

### A. Driver and Device Abstraction Layer (`kernel/drivers/driver.*`)
- Unified device model (`device_t`) and driver model (`driver_t`).
- Class categories: `DEVICE_TYPE_SYSTEM`, `DEVICE_TYPE_PCI`, `DEVICE_TYPE_INPUT`, `DEVICE_TYPE_BLOCK`, `DEVICE_TYPE_DISPLAY`, `DEVICE_TYPE_TIMER`, `DEVICE_TYPE_NETWORK`, `DEVICE_TYPE_USB`.
- Thread-safe device registration with spinlock protection, name/type queries, and inventory dumping.

### B. ACPI Discovery & Parsing (`kernel/hardware/acpi.*`)
- RSDP discovery from bootloader response with fallback physical memory scan in EBDA and BIOS ROM.
- Strict 8-bit checksum validation for RSDP, XSDT, RSDT, and all SDT headers.
- Multi-table parser:
  - MADT ("APIC"): Extracts Local APIC physical address, Processor Local APIC IDs, I/O APIC physical MMIO addresses and GSI bases, and Interrupt Source Overrides.
  - HPET ("HPET"): Extracts Event Timer Block ID, base MMIO address, sequence number, and minimum clock tick.

### C. PCI / PCIe Bus Discovery (`kernel/hardware/pci.*`)
- Type 1 PCI Configuration Space I/O access via ports `0xCF8` and `0xCFC`.
- Bus enumeration across buses 0..255, slots 0..31, and functions 0..7.
- Multifunction host controller detection.
- Safe, non-destructive BAR size probing restoring original register values.
- Device registration linking discovered PCI peripherals into the Driver Manager registry.

### D. Unified Input Subsystem (`kernel/drivers/input.*`)
- Centralized event queue (`input_event_t`) supporting key press/release and mouse move/button events.
- Virtual keycodes covering standard keyboard keys, function keys, and modifiers.
- Modifier tracking (`Shift`, `Ctrl`, `Alt`, `CapsLock`).
- Virtual keycode to ASCII translation.

### E. PS/2 Keyboard Driver (`kernel/drivers/ps2_kbd.*`)
- Hardware interrupt handling on IRQ 1 (vector `0x21`) with APIC EOI.
- Scancode Set 1 make and break code translation.
- Extended `0xE0` scancode tracking for cursor, editing, and navigation keys.
- Real-time modifier tracking and event queue dispatch.

### F. PS/2 Mouse Driver (`kernel/drivers/ps2_mouse.*`)
- Auxiliary device initialization, configuration byte setup, and data reporting enabled (`0xF4`).
- Hardware interrupt handling on IRQ 12 (vector `0x2C`) with APIC EOI.
- 3-byte packet state machine with protocol bit 3 sanity verification.
- 9-bit signed relative X/Y movement calculation and button transition tracking.

### G. Generic Block Storage Abstraction (`kernel/drivers/block.*`)
- Uniform `block_device_t` interface with sector-granular read/write/flush operations.
- 64-bit LBA bounds verification, arithmetic overflow checking (`chk_add_u64`), and read-only protection.

### H. RAM Disk Block Storage (`kernel/drivers/ramdisk.*`)
- In-memory RAM disk (`ramdisk0`) configured with 2048 sectors (1 MiB) for safe storage testing.

### I. VirtIO Block Storage Driver (`kernel/drivers/virtio_blk.*`)
- PCI discovery of QEMU VirtIO block devices (`0x1AF4:0x1001` legacy, `0x1AF4:0x1042` modern).
- vring queue configuration (Descriptor table, Available ring, Used ring) with physical page frame programming.
- Synchronous polling sector read and write operations.
- Block device registration as `"vda"`.

### J. USB Subsystem Architecture (`kernel/drivers/usb.*`)
- Host controller discovery for UHCI, OHCI, EHCI, and xHCI via PCI class `0x0C:0x03`.
- Binary descriptor definitions per USB 2.0 / 3.0 specification: Setup packet (8B), Device descriptor (18B), Config descriptor (9B), Interface descriptor (9B), Endpoint descriptor (7B).

### K. Structured Hardware Diagnostics (`kernel/hardware/hw_report.*`)
- Structured diagnostic report querying real CPUID, PMM memory, ACPI/MADT, Local APIC, PCI bus inventory, input devices, block storage devices, and GOP framebuffer.

---

## 3. Hardware Self-Test Verification

All subsystems pass automated in-kernel and external verification:
- `acpi_run_selftests`: **PASS** (ACPI table checksumming, MADT parsing, and discovery verified)
- `pci_run_selftests`: **PASS** (PCI config space, bus enumeration, and BAR probing verified)
- `input_run_selftests`: **PASS** (event queue, scancode translation, modifiers, and mouse packets verified)
- `block_run_selftests`: **PASS** (sector read/write, multi-block I/O, bounds, and read-only checks verified)
- `usb_run_selftests`: **PASS** (standard descriptor packing, setup packets, and controller models verified)
- Standalone test suite (`tests/test_hardware.py`): **100% OK** (All 7 test suites pass).

---

## 4. Hardware Target Compliance (Intel Celeron J3355)

- Zero heavy background tasks or memory footprint spikes.
- Input queue drops oldest event on overflow, preventing system lockups.
- All storage tests execute on memory-backed ramdisks or virtual disk images without touching host Windows or physical disk sectors.
