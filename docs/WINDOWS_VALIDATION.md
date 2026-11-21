> **Applies to the 0.0.0 "Genesis" validation bundle.** The repository is now at 1.0.0; no 1.0.0 Windows bundle exists yet. If you run a 1.0.0 image in QEMU on the 4 GB PC, use `--mem 256M` or less, never 4G.

# Windows 11 Validation Guide — Intel Celeron J3355, 4 GB RAM

**Purpose:** confirm that ZERO OS 0.0.0 boots in an emulator on *your* machine, and measure what that costs it.
**Nothing here installs ZERO OS on the PC.** The kernel only ever runs inside QEMU, from a disk-image *file*.

## Ground rules (read first)

- **Do not write any `.img` file to a USB stick, SSD, HDD or partition.** Do not use Rufus, `dd`, `diskpart`,
  Balena Etcher or similar. ZERO OS 0.0.0 is not an installable system.
- Nothing below touches Windows partitions, the boot loader, or your files. Path A changes nothing about Windows
  except installing one normal application (QEMU). Path B (optional) enables a Windows feature — see the disclosure there.
- Stop at any step that does not behave as described and send me the output. Do not disable antivirus/SmartScreen.
- When reporting back, paste only what the guide asks for. Do not paste your user name, product keys or file paths outside the validation folder.

## What was measured (so you know what to expect)

Measured on a Linux cloud VM (2 vCPU, software emulation, no KVM) — **not your machine**, so expect your J3355 to be slower:

| Item | Measured |
|------|----------|
| Smallest guest RAM that boots | 128 MB (96 MB hangs in firmware) |
| QEMU process peak memory | ≈ 190 MB, independent of guest size (128–512 MB tested) |
| Full test suite with a 256 MB guest | passes (result in `docs/VALIDATION_REPORT_0.0.0.md`) |
| Boot to "boot complete" | ≈ 2.7 s |

**Resource-conscious settings used throughout:** `-m 256M` (2× the minimum), `-smp 1` (one emulated CPU),
`-accel tcg` (pure software emulation: no Hyper-V/WHPX needed), `-display none` for the measured run
(no window to draw), and `snapshot=on` (the image file can never be modified).

> Status of this guide: the QEMU arguments are the same ones verified on Linux (see the report). The PowerShell
> wrappers and the Windows QEMU install were **not run on Windows** — I have no Windows machine. Treat any
> mismatch as a guide bug and report it.

---

## Path A (recommended): boot the prebuilt image in QEMU for Windows

Needs about 1 GB free disk (QEMU for Windows) and ~300 MB free RAM during the run. No hypervisor, no WSL, no reboot.

### A0. Prepare the machine
1. Close the browser and other heavy apps. Leave Task Manager open (Performance → Memory).
2. Check free memory in PowerShell. You want **at least 800 MB free**:
   ```powershell
   $os = Get-CimInstance Win32_OperatingSystem
   "{0} build {1}" -f $os.Caption, $os.BuildNumber
   "RAM total {0:N0} MB, free {1:N0} MB" -f ($os.TotalVisibleMemorySize/1KB), ($os.FreePhysicalMemory/1KB)
   (Get-CimInstance Win32_Processor | Select-Object -First 1).Name.Trim()
   ```
   If free memory is below ~800 MB, close more applications and re-check.

### A1. Install QEMU (one normal application install; asks for administrator approval once)
```powershell
winget search qemu                                  # confirm the package id first
winget install --id SoftwareFreedomConservancy.QEMU -e
```
(or download the installer from https://qemu.weilnetz.de/w64/ and run it). Then:
```powershell
$q = "C:\Program Files\qemu\qemu-system-x86_64.exe"
& $q --version | Select-Object -First 1
```
Expected: a QEMU version line (8.x or newer). If the path differs, locate it with `Get-Command qemu-system-x86_64` or search Program Files.

### A2. Unpack the validation bundle
Unzip `ZERO-OS-0.0.0-validation-bundle.zip` (delivered with this guide) to a plain folder, e.g. `C:\ZEROVAL`, and:
```powershell
cd C:\ZEROVAL
Get-FileHash -Algorithm SHA256 .\zero-os-validate.img, .\zero-os-interactive.img, .\OVMF_CODE_4M.fd, .\OVMF_VARS_4M.fd |
    Format-Table Hash, Path -AutoSize
Get-Content .\SHA256SUMS.txt
```
Every hash must equal the matching line in `SHA256SUMS.txt` (PowerShell prints upper-case, the file lower-case — compare ignoring case).
If any differs, **stop** and tell me.

The bundle contains: `zero-os-validate.img` (kernel exits QEMU automatically with a status code), `zero-os-interactive.img`
(just boots and stays up), `OVMF_CODE_4M.fd` + `OVMF_VARS_4M.fd` (UEFI firmware, the same files my tests use),
`SHA256SUMS.txt`, `README.txt` (the commit it was built from).

### A3. Measured, headless validation run
```powershell
cd C:\ZEROVAL
Copy-Item .\OVMF_VARS_4M.fd .\vars-run.fd -Force      # fresh, writable firmware variables
Remove-Item .\zero-serial.log -ErrorAction SilentlyContinue

$qargs = @(
  '-machine','q35','-cpu','qemu64','-m','256M','-smp','1','-accel','tcg',
  '-drive','if=pflash,format=raw,readonly=on,file=OVMF_CODE_4M.fd',
  '-drive','if=pflash,format=raw,file=vars-run.fd',
  '-drive','file=zero-os-validate.img,format=raw,if=virtio,snapshot=on',
  '-device','isa-debug-exit,iobase=0xf4,iosize=0x04',
  '-display','none','-serial','file:zero-serial.log','-no-reboot'
)
$sw   = [Diagnostics.Stopwatch]::StartNew()
$p    = Start-Process -FilePath $q -ArgumentList $qargs -PassThru -WindowStyle Hidden
$null = $p.Handle                                      # keeps the exit code readable on Windows PowerShell 5.1
$peak = 0
while (-not $p.HasExited -and $sw.Elapsed.TotalSeconds -lt 180) {
    $p.Refresh(); if ($p.WorkingSet64 -gt $peak) { $peak = $p.WorkingSet64 }
    Start-Sleep -Milliseconds 250
}
if (-not $p.HasExited) { $p.Kill(); "TIMEOUT: still running after 180 s - killed" }
$p.WaitForExit()
"exit code: {0}   wall: {1:N1} s   QEMU peak working set: {2:N0} MB" -f $p.ExitCode, $sw.Elapsed.TotalSeconds, ($peak/1MB)
```
**Pass = exit code `33`.** (`35` means the kernel panicked; anything else/timeout means it did not boot.)
Then check the kernel's own log:
```powershell
Get-Content .\zero-serial.log | Select-String 'ZERO OS 0.0.0','bootloader:','long mode','gdt:','idt:','boot complete'
```
Expected lines (values like CR3 and memory sizes may differ slightly):
```
  ZERO OS 0.0.0 "Genesis"
[INFO ] boot: bootloader: Limine 11.4.1 (base revision 6)
[INFO ] cpu: EFER=0xd00 (long mode active, NX enabled)
[INFO ] gdt: loaded: CS=0x08 DS=0x10 TSS=0x18, #DF stack IST1 at 0xffffffff8000a070
[INFO ] idt: loaded: 32 exception vectors, #DF on IST1, interrupts remain disabled
[INFO ] boot: ZERO-OS 0.0.0 boot complete
```
Because of `snapshot=on` the image is unchanged afterwards; you can re-run the hash check to prove it.

### A4. Optional: see the boot screen (uses more CPU/RAM — skip if the machine is struggling)
```powershell
Copy-Item .\OVMF_VARS_4M.fd .\vars-run.fd -Force
& $q -machine q35 -cpu qemu64 -m 256M -smp 1 -accel tcg `
  -drive if=pflash,format=raw,readonly=on,file=OVMF_CODE_4M.fd `
  -drive if=pflash,format=raw,file=vars-run.fd `
  -drive file=zero-os-interactive.img,format=raw,if=virtio,snapshot=on `
  -no-reboot
```
A window opens; after a few seconds it shows the ZERO banner (graphite panel, crimson edge, gold rule) and the boot log.
The kernel then halts by design; **close the window** to end the run.

### A5. Report back (copy these into one message)
1. The A0 output (Windows build, CPU name, RAM total/free).
2. The QEMU version line.
3. The A3 result line (`exit code … wall … peak working set …`).
4. The output of the `Select-String` command above.
5. If anything failed: the last 30 lines of `zero-serial.log` (`Get-Content .\zero-serial.log -Tail 30`).

### Troubleshooting (Path A)
| Symptom | Likely cause / action |
|---------|----------------------|
| `-accel` not recognised | older QEMU: replace `'-accel','tcg'` with `'-machine','q35,accel=tcg'` (and drop the first `'-machine','q35'`) |
| exit code `1`/immediate exit | a path in `$qargs` is wrong (run from `C:\ZEROVAL`); or firmware file missing — re-check A2 |
| no `zero-serial.log` | QEMU never started: run the command directly in the console (`& $q @qargs`) to see its error text |
| TIMEOUT | the J3355 may be slow under emulation; rerun with the 180 in the loop raised to 600; if it still times out, send `zero-serial.log` |
| SmartScreen/Defender blocks the zip or QEMU | do not disable it; tell me what it says |

---

## Path B (optional, heavier): build and run the full test suite yourself via WSL2

Only do this if you want to validate the *build* workflow. It validates the same kernel, from source.

> **Disclosure — read before enabling.** WSL2 runs inside Windows' Virtual Machine Platform. Enabling it makes
> Windows itself run on top of the hypervisor and typically sets the hypervisor to launch at boot (the
> `hypervisorlaunchtype` entry in Windows' own boot configuration). It is a standard, reversible Windows feature
> installed by Microsoft's own tooling — ZERO OS does not touch your bootloader or partitions — but because you asked
> me to keep boot configuration untouched, **Path A needs none of this and is the recommended path.** If you are not
> comfortable, stop here.

Memory/CPU limits matter on 4 GB. Create `%UserProfile%\.wslconfig` **before** installing:
```ini
[wsl2]
memory=1536MB
processors=2
swap=1GB
```
(Starting values: the work needs well under 1 GB — QEMU ≈ 190 MB plus a small compiler. If Windows becomes sluggish, use `processors=1`.)

1. Check virtualization is enabled in firmware: `(Get-CimInstance Win32_Processor).VirtualizationFirmwareEnabled` → `True`. If `False`, stop (that needs a BIOS/UEFI setting; ask me first).
2. Install (administrator PowerShell, reboot when asked): `wsl --install -d Ubuntu-24.04`
3. In the Ubuntu terminal:
   ```bash
   sudo apt-get update
   sudo apt-get install -y --no-install-recommends clang lld nasm make git qemu-system-x86 ovmf mtools gdisk python3 gdb
   ```
   (about 1 GB of downloads/disk.)
4. Put the source on the **Linux** filesystem (not `/mnt/c`: it is slow and loses file permissions):
   ```bash
   cp /mnt/c/Users/<you>/Downloads/ZERO-OS-0.0.0.tar.gz ~/ && tar xzf ~/ZERO-OS-0.0.0.tar.gz -C ~ && cd ~/ZERO-OS
   scripts/check-env.sh                    # must end with: RESULT: all dependencies present.
   make                                    # expect: built build/zero-kernel.elf
   ```
5. Light check first, then the full suite (the full suite took ≈ 2.5 minutes on the 2-vCPU sandbox; expect several times that on a J3355):
   ```bash
   ZERO_TEST_MEM=256M python3 tests/run_tests.py --only version boot exception_df wx
   ZERO_TEST_MEM=256M python3 tests/run_tests.py      # expect: 16 passed, 0 failed
   ```
6. Afterwards free the memory: in PowerShell `wsl --shutdown`. To remove everything: `wsl --unregister Ubuntu-24.04`.
7. Report: the `check-env.sh` result, the last line of each test run, and `ZERO_TEST_MEM=256M` timing (`time` output).

## What this validation does and does not prove

Proves: the kernel boots in QEMU under *your* Windows machine's QEMU, produces the expected diagnostics, and the emulation fits
a 4 GB machine. Does **not** prove: anything about booting ZERO OS on the real J3355 hardware (not attempted, not recommended
yet), real-hardware drivers, or Secure Boot. Real-hardware testing is a separate, explicit decision for a later milestone (`HARDWARE.md`).
