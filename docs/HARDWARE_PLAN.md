# Hardware Compatibility Plan and Driver Strategy

**Status: plan. Only COM1 and the Limine/GOP framebuffer exist in 0.0.0.**

## Principles

- Every driver gets a row in `HARDWARE.md` with: status (none / emulator-only / tested-on-hardware),
  tested device IDs, known issues. "Works in QEMU" is never reported as "works".
- No driver is assumed to exist for any device; absent hardware degrades gracefully.
- Drivers sit behind a HAL interface so QEMU-friendly (virtio) and real-hardware drivers coexist.

## Order (3.0.0 unless noted)

1. PCIe enumeration, ACPI table parsing (needs 1.0.0 memory manager).
2. Input: PS/2 keyboard/mouse (QEMU-friendly), then USB xHCI + HID (J3355 has xHCI).
3. Storage: AHCI (SATA) and NVMe; virtio-blk for QEMU. Needed by 2.0.0's VFS for persistence.
4. Display: UEFI GOP framebuffer (already available). Native Intel HD 500 (Gen9) modesetting is
   a **research item**; acceleration is not promised.
5. Network: Intel Ethernet (e1000e/igb class) after storage; virtio-net for QEMU. Wi-Fi: **not planned** until
   after 6.0.0 (firmware blobs, licensing, WPA stack).
6. Audio: Intel HDA — after USB.

## Target-hardware validation

First physical boot (3.0.0) will be from a USB stick on the user's machine, only with explicit user action;
internal disks and boot configuration stay untouched during early development. Secure Boot must be
off (or the loader signed) — see ARCHITECTURE.md conflict #4.
