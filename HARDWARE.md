# Hardware Support

Rule: **no support is claimed without a test.** "Tested" below means a recorded run of
`tests/run_tests.py`, on the stated platform only.

## Target

Intel Celeron J3355 (Apollo Lake, 2 cores), 4 GB RAM, Intel HD Graphics 500, UEFI x86_64.
**Neither 0.0.0 nor 1.0.0 has been run on this hardware.** The development sandbox could not inspect it.

## Support matrix

| Component | Status in 1.0.0 | Evidence |
|-----------|-----------------|----------|
| x86_64 CPU, long mode | Works (QEMU `qemu64`) | boot test: `EFER.LMA` active |
| UEFI boot via Limine | Works (QEMU + OVMF, Secure Boot off) | boot test |
| COM1 serial (16550) | Works (QEMU) | all tests read the serial log; **physical J3355 PCs may have no COM1 port** |
| GOP framebuffer, 32-bpp RGB | Works (QEMU 1280x800) | framebuffer screenshot test |
| Memory map | Validated and classified; bitmap PMM allocates only usable memory (QEMU, 128 MB–4 GB guests). **Real J3355 memory map never seen**; its firmware map may differ (more regions, ACPI/reserved layout) | `pmm_selftests_*`, memmap tests |
| Physical RAM (4 GB target) | PMM bitmap would be about 128 KiB for 4 GiB of address span (computed; **not measured on that machine**). Bootloader-reclaimable memory is not reclaimed (KNOWN LIMITATION) | design |
| Keyboard, mouse, storage, USB, audio, Ethernet, Wi-Fi, GPU acceleration | **Not implemented** | — |
| Real J3355 hardware | **Untested** | — |
| Secure Boot | **Unsupported** (Limine unsigned) | — |

Planned drivers and their order are in `docs/HARDWARE_PLAN.md`.
