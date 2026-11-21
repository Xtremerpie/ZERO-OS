#!/usr/bin/env bash
# Report which ZERO OS build/test dependencies are present or missing (Linux / WSL2).
# Prints tool versions only - no usernames, hostnames, serial numbers or paths beyond tool locations.
set -u

missing=0
need() {  # need <command> <what-it-is> <apt-package>
    if command -v "$1" >/dev/null 2>&1; then
        v="$("$1" --version 2>&1 | head -1)"
        printf '  [ ok ] %-22s %s\n' "$1" "$v"
    else
        printf '  [MISS] %-22s %s  (apt: %s)\n' "$1" "$2" "$3"
        missing=$((missing + 1))
    fi
}

echo "ZERO OS environment check"
echo "  platform: $(uname -sm)   cpus: $(nproc)   ram: $(free -m 2>/dev/null | awk '/Mem:/ {print $2" MB"}')"
echo "Build:"
need clang   "C compiler (freestanding x86_64 target)" clang
need ld.lld  "linker"                                  lld
need nasm    "assembler"                               nasm
need make    "build driver"                            make
need git     "version control"                         git
echo "Image + test:"
need qemu-system-x86_64 "emulator"        qemu-system-x86
need sgdisk             "GPT partitioning" gdisk
need mformat            "FAT image tools"  mtools
need python3            "test harness"     python3
echo "Debug (optional):"
need gdb "debugger" gdb

printf '  firmware: '
found=""
for d in /usr/share/OVMF /usr/share/ovmf /usr/share/edk2/ovmf /usr/share/edk2/x64 /usr/share/qemu; do
    for f in OVMF_CODE_4M.fd OVMF_CODE.fd edk2-x86_64-code.fd; do
        [ -f "$d/$f" ] && found="$d/$f" && break 2
    done
done
if [ -n "$found" ]; then echo "[ ok ] OVMF UEFI firmware: $found"; else echo "[MISS] OVMF UEFI firmware (apt: ovmf)"; missing=$((missing + 1)); fi

[ -e /dev/kvm ] && echo "  note: /dev/kvm present (QEMU can use KVM)" || echo "  note: no /dev/kvm - QEMU uses software emulation (slower, still works)"

if [ "$missing" -gt 0 ]; then
    echo "RESULT: $missing dependency/dependencies missing."
    echo "Debian/Ubuntu: sudo apt-get install -y clang lld nasm make git qemu-system-x86 ovmf mtools gdisk python3"
    exit 1
fi
echo "RESULT: all dependencies present."
