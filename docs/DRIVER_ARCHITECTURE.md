# ZERO OS — Driver & Hardware Abstraction Architecture

**Milestone:** 3.0.0 Hardware Foundation  

---

## 1. Overview

The ZERO OS Driver Architecture decouples hardware-specific register manipulation from higher-level operating system subsystems (VFS, process manager, and user space). Every peripheral is modeled as a `device_t` registered within a centralized registry managed by the Driver Manager.

```
   ┌────────────────────────────────────────────────────────┐
   │         User Applications & System Services            │
   └───────────────────────────┬────────────────────────────┘
                               │ Syscalls / VFS
   ┌───────────────────────────▼────────────────────────────┐
   │             Virtual File System / Event APIs           │
   └─────────────┬────────────────────────────┬─────────────┘
                 │                            │
   ┌─────────────▼──────────────┐ ┌───────────▼─────────────┐
   │ Block Device Abstraction   │ │ Unified Input Subsystem │
   │ (block_device_t)           │ │ (input_event_t queue)   │
   └───────┬────────────┬───────┘ └─────┬─────────────┬─────┘
           │            │               │             │
   ┌───────▼────┐ ┌─────▼───────┐ ┌─────▼─────┐ ┌─────▼─────┐
   │ VirtIO Blk │ │   RAM Disk  │ │  PS/2 Kbd │ │PS/2 Mouse │
   └───────┬────┘ └─────────────┘ └─────┬─────┘ └─────┬─────┘
           │                            │             │
   ┌───────▼────────────────────────────▼─────────────▼─────┐
   │        Driver Manager & Device Registry (device_t)     │
   └───────┬────────────────────────────┬───────────────────┘
           │                            │
   ┌───────▼────────────────────┐ ┌─────▼───────────────────┐
   │     PCI / PCIe Discovery   │ │   ACPI Firmware Parser  │
   └───────┬────────────────────┘ └─────┬───────────────────┘
           │ Port I/O (0xCF8/CFC)       │ RSDP / XSDT / MADT
   ┌───────▼────────────────────────────▼───────────────────┐
   │            Hardware / Firmware (QEMU / Bare-Metal)      │
   └────────────────────────────────────────────────────────┘
```

---

## 2. Core Abstractions

### A. Device Model (`device_t`)
Every device tracked in the system contains:
- `name`: Human-readable device identifier (e.g. `"PS/2 Keyboard"`, `"vda"`, `"ramdisk0"`).
- `type`: Category (`DEVICE_TYPE_SYSTEM`, `DEVICE_TYPE_PCI`, `DEVICE_TYPE_INPUT`, `DEVICE_TYPE_BLOCK`, `DEVICE_TYPE_DISPLAY`, `DEVICE_TYPE_TIMER`, `DEVICE_TYPE_NETWORK`, `DEVICE_TYPE_USB`).
- `state`: Lifecycle status (`DEVICE_STATE_UNINITIALIZED`, `DEVICE_STATE_ACTIVE`, `DEVICE_STATE_FAILED`, `DEVICE_STATE_DISABLED`).
- `resources`: Up to 8 tracked hardware resources (`RES_IO_PORT`, `RES_MMIO`, `RES_IRQ`).
- `driver`: Pointer to `driver_t` lifecycle operations (`init`, `start`, `stop`, `handle_irq`).

### B. Block Device Model (`block_device_t`)
Storage controllers implement the `block_device_ops_t` table:
- `read_blocks(dev, lba, count, buffer)`: Reads `count` sectors starting at `lba`.
- `write_blocks(dev, lba, count, buffer)`: Writes `count` sectors starting at `lba`.
- `flush(dev)`: Flushes write caches.

### C. Unified Input Model (`input_event_t`)
Input hardware converts raw scancodes or serial packets into unified events:
- `type`: `INPUT_KEY_DOWN`, `INPUT_KEY_UP`, `INPUT_MOUSE_MOVE`, `INPUT_MOUSE_BUTTON_DOWN`, `INPUT_MOUSE_BUTTON_UP`.
- `code`: Standard virtual keycode or button bitmask.
- `dx`, `dy`: Relative mouse movement deltas.
- `modifiers`: Bitfield tracking `MODIFIER_SHIFT`, `MODIFIER_CTRL`, `MODIFIER_ALT`, `MODIFIER_CAPSLOCK`.

---

## 3. Supported Controllers in 3.0.0

1. **ACPI Subsystem:** Discovers RSDP via Limine or memory search, parses XSDT/RSDT, extracts Local APIC & IO-APIC topology from MADT, discovers HPET.
2. **PCI Enumerator:** Scans buses 0..255, slots 0..31, functions 0..7. Safely sizes BARs by saving, masking with `0xFFFFFFFF`, and restoring original registers.
3. **PS/2 Controller:** Interacts with ports `0x60` and `0x64`. Drives keyboard interrupts on IRQ 1 and mouse interrupts on IRQ 12.
4. **RAM Disk:** Memory-backed block device for isolated tests and root image staging.
5. **VirtIO Block:** Interacts with QEMU VirtIO block devices (`0x1AF4:0x1001`/`0x1042`), configuring vrings and performing asynchronous or polled block transfers.
6. **USB Architecture:** PCI discovery identifies UHCI, OHCI, EHCI, and xHCI controllers and models USB setup packets and standard descriptors.
