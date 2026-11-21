# Boot Sequence

## Implemented in 0.0.0 [implemented+tested]

| Step | Actor | What happens |
|------|-------|--------------|
| 1 | UEFI firmware (OVMF) | Initialises, finds the ESP, runs `\EFI\BOOT\BOOTX64.EFI` |
| 2 | Limine 11.4.1 | Reads `/limine.conf` (`timeout: 0`, `serial: yes`, `quiet: yes`), loads `/boot/zero-kernel.elf` |
| 3 | Limine | Enters 64-bit long mode, maps the kernel in the higher half and builds the HHDM (`0xffff800000000000` observed), collects memory map + framebuffer, jumps to `kmain` |
| 4 | `kmain` | `serial_init()` (COM1 loopback self-check) → serial banner |
| 5 | `kmain` | Verifies the Limine base-revision handshake (requested 6; loaded 6 observed). If unsupported: logs, halts |
| 6 | `kmain` | Framebuffer: banner + log console (skipped, serial-only, if no 32-bpp RGB framebuffer) |
| 7 | `kmain` | Logs version/build id, bootloader name/version, cmdline; `selftest_configure()` parses `zero.*` options |
| 8 | `cpu_report` | CPUID vendor/brand/family, CR0/CR3/CR4, EFER (long-mode + NX flags) |
| 9 | `gdt_init`, `idt_init` | Own GDT + TSS (IST1 for #DF), own IDT for vectors 0–31 |
| 10 | `kmain` | HHDM offset, memory-map summary (reporting only) |
| 11 | `selftest_run` | If requested, injects a fault (see `kernel/include/zero/selftest.h`) |
| 12 | `kmain` | Logs "boot complete"; QEMU exit signal if `zero.test_exit=1`; otherwise `cli; hlt` loop |

## Boot-failure handling

- No serial port (loopback fails): continue; framebuffer still reports.
- No usable framebuffer: continue serial-only, logged as a warning.
- Unsupported Limine base revision: logged, CPU halted.
- Any CPU exception other than #BP: panic with register dump, then halt (`kernel/debug/panic.c`).
- Bootloader-stage failures (bad kernel file, missing config): Limine's own behaviour; the test
  harness detects "no kernel output" but cannot yet say *why* (limit documented in TESTING.md).

## Designed, not implemented [designed]

Memory-map consumption by a physical memory manager (1.0.0); early-boot timing
via TSC/HPET for boot-duration measurement (1.0.0); recovery entry in `limine.conf` (2.0.0).
