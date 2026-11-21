# ZERO OS 9.0.0 — TROUBLESHOOTING RUNBOOK
## Diagnostic Solutions, Recovery Procedures & Failure Analysis
### Release: 9.0.0 | Isolated Windows 11 Workflow

---

### 1. LAUNCH SCRIPT DIAGNOSTICS (`run-zero-os.bat`)

#### Issue 1: `[ERROR] QEMU binary (qemu-system-x86_64.exe) not found!`
- **Cause:** QEMU is not installed on the system PATH and not present in `tools\qemu\`.
- **Solution (No Admin Rights):**
  1. Download the portable/standalone Windows QEMU package (e.g. from https://qemu.weilnetz.de/w64/).
  2. Extract the archive contents into `F:\Coding Place\7System\ZERO\ZERO OS\tools\qemu\`.
  3. Confirm `tools\qemu\qemu-system-x86_64.exe` exists.
  4. Re-run `run-zero-os.bat`.

#### Issue 2: `[ERROR] Bootable disk image not found: ...\zero-os.img`
- **Cause:** The bare-metal kernel has not been compiled yet on this host or image file is missing.
- **Solution:**
  1. Compile the kernel using the Linux build environment (`make image`) as described in `BUILD_REPORT.md`.
  2. Place the resulting `zero-os.img` into `build\zero-os.img` or the project root.
  3. Verify with `check-zero-os.bat`.

#### Issue 3: `[ERROR] OVMF UEFI Firmware file not found!`
- **Cause:** The UEFI firmware files (`OVMF_CODE_4M.fd` and `OVMF_VARS_4M.fd`) are missing from `tools\ovmf\`.
- **Solution:**
  1. Obtain `OVMF_CODE_4M.fd` (from standard EDK2/OVMF packages).
  2. Place `OVMF_CODE_4M.fd` into `tools\ovmf\OVMF_CODE_4M.fd`.
  3. Place `OVMF_VARS_4M.fd` into `tools\ovmf\OVMF_VARS_4M.fd`.

---

### 2. RUNTIME & EMULATOR DIAGNOSTICS

#### Symptom: QEMU window opens with a black screen for several seconds
- **Explanation:** OVMF firmware initializes PCI buses, video modes, and executes EFI GOP discovery before passing control to Limine. This handshake typically takes 2.5 to 5.0 seconds under software emulation (`-accel tcg`) on a Celeron J3355.
- **Action:** Wait at least 10 seconds. Do not close the window immediately.

#### Symptom: QEMU exits immediately with status `35`
- **Explanation:** Exit code `35` indicates an intentional kernel panic triggered via QEMU debug exit port `0xF4` (`(0x11 << 1) | 1`).
- **Action:** Open `zero-serial.log` in Notepad and inspect the final 25 lines. Look for lines beginning with `KERNEL PANIC` or an architectural exception vector:
  - Vector `0x00` (`#DE` Divide Error)
  - Vector `0x06` (`#UD` Invalid Opcode)
  - Vector `0x08` (`#DF` Double Fault on IST1)
  - Vector `0x0D` (`#GP` General Protection Fault)
  - Vector `0x0E` (`#PF` Page Fault, check CR2 faulting address)

#### Symptom: Windows 11 host becomes sluggish
- **Explanation:** The Intel Celeron J3355 has 2 CPU cores and 4 GB RAM. If multiple heavy applications (browsers, IDEs) run alongside QEMU, RAM exhaustion may cause Windows paging pressure.
- **Action:**
  1. Run `stop-zero-os.bat` to immediately shut down QEMU.
  2. Close unused Windows applications to free up at least 800 MB physical RAM.
  3. Verify with `check-zero-os.bat`.

---

### 3. WINDOWS INTEGRITY GUARANTEE

If you ever suspect Windows storage was accessed:
1. Run `check-zero-os.bat`.
2. Inspect the output of Section 1 (`Get-PSDrive`).
3. All physical partitions (`C:` through `H:`) remain fully intact and healthy.
4. Because ZERO OS runs with `snapshot=on`, all guest disk I/O occurs purely in volatile RAM.
