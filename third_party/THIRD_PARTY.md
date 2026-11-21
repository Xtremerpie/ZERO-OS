# Third-Party Dependencies

Every external component used by ZERO OS is listed here. None of them is a
kernel. The ZERO OS kernel (`kernel/`) is original code.

| Component | What | Version / commit | License | Where |
|-----------|------|------------------|---------|-------|
| Limine bootloader | `BOOTX64.EFI` (UEFI bootloader binary, prebuilt) | branch `v11.x-binary`, commit `5be26a73d7b7b4d4477d18be94e1d16e615adf56` | BSD 2-Clause (`third_party/limine/LICENSE`) | `third_party/limine/BOOTX64.EFI` |
| limine-protocol | `limine.h` (boot protocol header, used by the kernel at compile time) | commit `6e85ddd6f928b5b512015c4d47d1cfce2fd28079` | 0BSD (`third_party/limine/LICENSE.limine-protocol`) | `third_party/limine/limine.h` |
| font8x8 | 8x8 bitmap font (basic Latin) for the framebuffer banner | Daniel Hepper's `font8x8_basic.h`, based on IBM public-domain VGA fonts | Public Domain | `third_party/font8x8/font8x8_basic.h` (one edit: `char` array made `static const unsigned char`) |

SHA-256 of vendored files (verify with `sha256sum`):

```
333f7a69379b1f47e019be215885cdc078b9edd6e9255cbe2f706d6c5ec0ef06  third_party/limine/BOOTX64.EFI
0aadf2633c85d8cb145344c968add158aeb1deb68af567d55ba8431c492b6d4c  third_party/limine/limine.h
77f7eecdf7bc8144a318b94d3c3b04512356814d41cd86b505d1b360437d752f  third_party/font8x8/font8x8_basic.h
```

## Verification record

Re-verified 2026-11-14: `sha256sum -c` of the three vendored files matches the table; the binary and header
match the copies originally downloaded from the pinned upstream commits (HEAD of the clones confirmed to be the recorded commits); the font matches upstream apart from the one documented edit.
Limine's `LICENSE` is the BSD 2-Clause text (two numbered clauses present); the protocol header's licence is the 0BSD text.

## Build-time tools (not shipped, not vendored)

clang/LLVM (kernel compiler), ld.lld (linker), NASM (assembler), GNU Make,
QEMU + OVMF (testing), mtools, gdisk/sgdisk (disk image creation),
Python 3 (test harness), GDB (debugging). See `BUILDING.md` for tested versions.

## Policy

- Limine is the *bootloader only*. It loads our kernel and hands over a memory
  map, framebuffer and HHDM offset. It is not part of the kernel.
- Prebuilt third-party binaries are vendored with a pinned commit and hash so
  builds are repeatable offline. Replacing Limine with a ZERO-native bootloader
  is a possible later decision, not a 0.0.0 goal.
- Adding any dependency requires adding a row to this table in the same commit.
