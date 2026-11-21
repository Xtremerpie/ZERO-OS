# Building ZERO OS (current: 7.0.0)

## Verified environment

Everything below was run and verified on **Ubuntu 24.04** (x86_64, 2 vCPU, 8 GB RAM, no KVM):

| Tool | Version used |
|------|--------------|
| clang / ld.lld | 18.1.3 |
| NASM | 2.16.01 |
| GNU Make | 4.3 |
| QEMU | 8.2.2 (software emulation) |
| OVMF | Ubuntu `ovmf` package (`OVMF_CODE_4M.fd`) |
| mtools / sgdisk | 4.0.43 / 1.0.10 |
| Python | 3.13 |
| Limine (vendored) | 11.4.1, commit `5be26a73…` (see `third_party/THIRD_PARTY.md`) |

Install (Debian/Ubuntu): `sudo apt-get install -y clang lld nasm make git qemu-system-x86 ovmf mtools gdisk python3 gdb`

Check what is missing: `scripts/check-env.sh` (exit status 1 and a list if anything is absent).

## Build, run, test

```
make            # builds build/zero-kernel.elf  (clang, -Wall -Wextra -Werror)
make image      # builds build/zero-os.img       (GPT + FAT32 ESP: Limine + kernel)
make run        # boots the image in QEMU+OVMF   (serial log on this terminal)
make test       # full test suite (see TESTING.md)
make analyze    # clang static analyzer over all kernel sources (must report 0 findings)
make ubsan      # UBSan (trap mode) build in build-ubsan/; test it with: ZERO_BUILD_DIR=build-ubsan python3 tests/run_tests.py
make clean
```

Headless boot with a 4 GB guest (matching the target RAM): `scripts/run-qemu.sh build/zero-os.img --headless --mem 4G`
Debug with GDB (verified: source-level breakpoints at `kmain` and `exception_dispatch`):
terminal 1 `scripts/run-qemu.sh build/zero-os.img --headless --gdb`, terminal 2
`gdb build/zero-kernel.elf -ex 'target remote :1234' -ex 'hbreak kmain' -ex continue`
(use `hbreak`, hardware breakpoints, because QEMU starts paused inside firmware; the kernel is built with `-g`).

## Safety

`scripts/mkimage.sh` only writes a regular file you name, and **refuses block devices**. QEMU
runs the image as a virtual disk. Nothing in this repository writes to a physical disk,
partition table or boot configuration, and no step needs administrator rights.

## Windows 11 (your development machine) — NOT verified

I had no access to a Windows machine, so none of this was run there.

- **Recommended path:** WSL2 with Ubuntu, then the Linux instructions above unchanged.
  On a 4 GB machine, limit WSL2 memory (`%UserProfile%\.wslconfig` → `[wsl2]` `memory=2GB`) and
  use `--headless` (serial in the terminal) — the 1280x800 window needs WSLg.
  Nested virtualisation is not needed (QEMU uses software emulation, as in the verified run).
- `scripts/check-env.ps1` lists what is installed on Windows and what `winget` package to add.
  It is untested; its output may need adjustment.
- **Full step-by-step guide for the J3355 / 4 GB machine: `docs/WINDOWS_VALIDATION.md`.**
- A fully native Windows toolchain (MSYS2 + LLVM + NASM + QEMU) is plausible but the image
  script is Bash/mtools-based, so it would need a port. Not attempted in 0.0.0.

## Reproducibility

Inputs are pinned: Limine binary and protocol header and the font are vendored with SHA-256
hashes; the compiler and tool versions are listed above. Builds are **not** bit-for-bit
reproducible yet (the build id embeds `git describe` via a generated header, and FAT timestamps vary).
The build id is a real build dependency: it is regenerated whenever `git describe` changes and every
object that includes it is rebuilt, so a binary can never carry a stale id.

## 1.0.0 additions

- Include path `-Ikernel` lets kernel code include `memory/pmm.h`; no new tools or dependencies were added.
- `python3 tests/run_tests.py --matrix 128M,256M,512M,1G` selects PMM guest sizes (default). `4G` runs only if listed **and** the host has
  that much free memory; **never run a 4 GB guest on a 4 GB machine** (use `--matrix 128M,256M` there).
- `ZERO_PMM_MATRIX` sets the same list. Test disks are attached `snapshot=on`; the harness verifies the image hash is unchanged.
- Selftest kernel options: `zero.selftest=pmm | pmm_bench | memmap_overlap` (see `docs/MEMORY_MANAGEMENT.md`).
- Nothing in the build or tests touches a physical disk or the host boot configuration.
## 3.0.0 additions

- Added hardware abstraction layer under `kernel/drivers/` and `kernel/hardware/`.
- No new external host dependencies required for build logic.
- New test suite: `python3 tests/test_hardware.py` executes standalone algorithmic validation of ACPI checksums, MADT parsing, PCI configuration space, PS/2 scancodes, mouse packets, block device boundaries, and USB descriptor packing.
- Hardware reporting subsystem outputs formatted diagnostic summary to serial COM1 and GOP framebuffer console.

## 2.0.0 additions

- Subsystems added under `kernel/process/`, `kernel/syscall/`, `kernel/fs/`, `kernel/ipc/`, `kernel/loader/`.
- Assembly context switch routine `kernel/process/context.asm` and fast syscall stubs `kernel/syscall/syscall_entry.asm`.
