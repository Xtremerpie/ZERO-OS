#!/usr/bin/env bash
# Boot a ZERO OS disk image in QEMU with UEFI (OVMF). Serial log goes to this terminal.
#
#   scripts/run-qemu.sh <image> [--headless] [--mem 4G] [--monitor <unix-socket-path>] [--gdb]
#   --gdb  : QEMU waits for GDB on tcp::1234 (gdb build/zero-kernel.elf -ex "target remote :1234")
#
# Runs entirely inside QEMU. The disk image is attached with snapshot=on: guest writes go to a
# throw-away temp overlay, so the image file is never modified. It never touches a physical disk or boot configuration.
set -euo pipefail

IMG="${1:?usage: run-qemu.sh <image> [--headless] [--mem SIZE] [--monitor PATH] [--gdb]}"
shift || true
HEADLESS=0
MEM=512M
MONITOR=""
GDB=0
while [ $# -gt 0 ]; do
    case "$1" in
        --headless) HEADLESS=1 ;;
        --mem) MEM="$2"; shift ;;
        --monitor) MONITOR="$2"; shift ;;
        --gdb) GDB=1 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

find_ovmf() {
    for d in /usr/share/OVMF /usr/share/ovmf /usr/share/edk2/ovmf /usr/share/edk2/x64 /usr/share/qemu; do
        for f in OVMF_CODE_4M.fd OVMF_CODE.fd edk2-x86_64-code.fd; do
            [ -f "$d/$f" ] && { echo "$d/$f"; return; }
        done
    done
}
OVMF_CODE="$(find_ovmf)"
[ -n "$OVMF_CODE" ] || { echo "OVMF firmware not found (run scripts/check-env.sh)" >&2; exit 1; }

# Fresh writable copy of the firmware variable store each run (we `exec` QEMU below, so
# no cleanup trap can run; a fixed path under build/ avoids leaking temp files).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/build"
VARS="${ZERO_OVMF_VARS:-$ROOT/build/ovmf-vars.fd}"
dd if=/dev/zero of="$VARS" bs=1k count=$(( $(stat -c %s "$OVMF_CODE") / 1024 )) status=none
VARS_SRC="${OVMF_CODE/CODE/VARS}"
[ -f "$VARS_SRC" ] && cp "$VARS_SRC" "$VARS"

EXTRA_OPTS=()
[ "$HEADLESS" = 1 ] && EXTRA_OPTS+=(-display none)
[ "$GDB" = 1 ] && EXTRA_OPTS+=(-s -S)
[ -n "$MONITOR" ] && EXTRA_OPTS+=(-monitor "unix:$MONITOR,server,nowait")

exec qemu-system-x86_64 \
    -machine q35 -m "$MEM" -cpu qemu64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file="$VARS" \
    -drive file="$IMG",format=raw,if=virtio,snapshot=on \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -serial stdio -no-reboot "${EXTRA_OPTS[@]}"
