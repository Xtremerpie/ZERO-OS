# Performance Budgets and Measurements

Primary target: Intel Celeron J3355, 4 GB RAM, integrated graphics. **Nothing has been
measured on that machine.** Budgets are aspirational targets, not specifications.

## Budgets

| Metric | Target | Measurable from |
|--------|--------|-----------------|
| Idle memory | < 256 MB | 2.0.0 (services exist) |
| Boot time | < 15 s | 1.0.0 (timer) on real hardware |
| Idle CPU | minimal | 1.0.0 |
| Kernel memory footprint | track every milestone | now |
| Allocation / context switch / file ops / app startup / desktop responsiveness | benchmarks required by directive | 1.0.0 – 4.0.0 |

## Recorded results (0.0.0)

Environment: QEMU 8.2.2, software emulation (no KVM), `qemu64`, cloud VM. **Not representative of real hardware.**

| Measurement | Result |
|-------------|--------|
| Kernel ELF size (with debug info) | 72,264 bytes (`make`, build id `a7ca121-dirty` at measurement) |
| Kernel static sections (`size`) | text+rodata 15,364 B, data 336 B, bss 20,752 B (16 KiB is the #DF stack) |
| Whole QEMU run (OVMF + Limine + kernel to halt) | 2.2–3.9 s wall per boot across all recorded runs (first boot of a suite is the slowest); ~2.8–3.1 s typical |
| Usable RAM reported, 512 MB guest / 4 GB guest | ~461 MiB / 4045 MiB |

The 2.5 s figure is dominated by emulated firmware; it is **not** a ZERO boot time. The kernel has no
timer yet, so kernel-only boot duration cannot be measured. No regressions exist because there is no earlier baseline.
