# ZERO OS

**From Nothing. Built for Control.**

ZERO OS is an independent x86_64 operating-system project targeting UEFI systems and using the Limine boot protocol. The project contains a freestanding C kernel, x86_64 NASM assembly, a native desktop/application stack, storage and networking components, security services, and AI/automation infrastructure.

## Project status

- Official project release: **9.0.0 — ZERO Ultimate**
- Architecture: **x86_64**
- Boot design: **UEFI + Limine**
- Primary development target: Intel Celeron J3355, 4 GB RAM, Intel HD Graphics 500
- Kernel build and real QEMU boot still require independent verification. The generated disk image previously audited did not contain `/boot/zero-kernel.elf`; do not treat it as a boot-tested release.

## Source layout

- `kernel/` — kernel and architecture-specific code
- `desktop/`, `applications/` — graphical environment and native apps
- `drivers/`, `system/` — device and system services
- `intelligence/` — AI and automation components
- `sdk/` — application development kit
- `tests/` — automated project tests
- `tools/` — image and package utilities
- `third_party/limine/` — Limine boot files and headers
- `.github/workflows/` — CI workflows

## Build and test

See [BUILDING.md](BUILDING.md) and [TESTING.md](TESTING.md) for the supported build environment and test commands.

Typical Linux build commands:

```bash
make clean
make all
make test
```

The kernel must be compiled and embedded at `/boot/zero-kernel.elf` before attempting a UEFI/QEMU boot.

## Safety

Use an isolated QEMU virtual machine and disposable virtual disk images. Do not write ZERO OS to a physical disk or alter the host Windows boot configuration as part of routine development.
