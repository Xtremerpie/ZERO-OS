# ZERO OS 9.0.0 — FIRST BOOT GUIDE
## Isolated & Non-Destructive Windows 11 Workflow
### Architecture: x86_64 Higher-Half | Milestone: 9.0.0 ZERO Ultimate

---

### 1. ABSOLUTE WINDOWS SAFETY INVARIANTS

> [!IMPORTANT]
> **Your Windows 11 installation and physical hardware are 100% protected.**
> - **ZERO OS runs strictly inside an isolated virtual machine (QEMU).**
> - **Never writes to physical SSDs, hard drives, or USB media.**
> - **Never touches Windows drive letters (`C:`, `D:`, `E:`, `F:`, `G:`, `H:`).**
> - **Never touches the Windows EFI System Partition (ESP) or Windows Boot Manager (BCD).**
> - **Uses `snapshot=on`:** All guest writes are held in temporary memory and discarded upon shutdown.

---

### 2. ARCHITECTURAL OVERVIEW

ZERO OS is an independent, custom-built x86_64 higher-half operating system. It boots via UEFI and the Limine boot protocol, transitions to 64-bit long mode, initializes paging (`CR3`), GDT, IDT, Local APIC, and enters the kernel at `0xFFFFFFFF80000000`.

The visual shell features a 1280x800 graphical desktop environment built around the official color palette:
- **Deep Black (`#0A0A0C`)**: Compositor canvas & background.
- **Graphite (`#1E1E28`)**: Window frames, title bars & taskbar.
- **Metallic Gold (`#C9A227`)**: Borders, accents, highlights & HUD elements.
- **Crimson Red (`#B3202B`)**: Alert states, close buttons & laser projectiles.

---

### 3. GETTING STARTED ON WINDOWS 11

#### Step 1: Pre-Flight Audit
Run the automated pre-flight audit script:
```cmd
check-zero-os.bat
```
This verifies your Python environment, checks release manifest consistency, executes all 78 standalone milestone tests, and audits drive safety invariants.

#### Step 2: Providing the Bootable Image & QEMU
On your low-resource Windows 11 host (Celeron J3355, 4 GB RAM), compiling the bare-metal kernel requires the Linux cross-compiler toolchain (`clang --target=x86_64-unknown-none-elf`, `nasm`, and `make`).

To run ZERO OS in QEMU:
1. **QEMU Portable:** Place `qemu-system-x86_64.exe` and supporting DLLs into `tools\qemu\` (or install QEMU for Windows via winget).
2. **UEFI Firmware:** Place `OVMF_CODE_4M.fd` and `OVMF_VARS_4M.fd` into `tools\ovmf\`.
3. **Kernel Disk Image:** Place the compiled `zero-os.img` into `build\` or the project root.

#### Step 3: Launching ZERO OS
Double-click the one-click launcher:
```cmd
run-zero-os.bat
```
The script automatically:
- Validates that QEMU, OVMF, and `zero-os.img` exist.
- Creates a safe writable temporary copy of the UEFI variable store (`build\vars-run.fd`).
- Configures QEMU with lightweight parameters: 256 MB RAM, 1 vCPU, TCG software emulation, and snapshot mode.
- Launches the graphical virtual machine window and redirects kernel serial logs to `zero-serial.log`.

#### Step 4: Shutting Down Safely
- Simply close the QEMU window, or run:
```cmd
stop-zero-os.bat
```
Because the image is mounted with `snapshot=on`, all guest writes are discarded from memory. The virtual disk image remains pristine.

---

### 4. ZERO OS INTERACTIVE DESKTOP TOUR

Once booted, ZERO OS presents the native desktop environment:

#### 1. The Bottom Taskbar
- **Start Menu Button (Left):** Access registered system applications.
- **Window Switcher (Center):** Displays minimized and active windows.
- **System Tray (Right):** Digital clock, active performance profile (`NORMAL` / `GAMING`), master audio indicator, and system status icon.

#### 2. Native Applications (`kernel/desktop/app.c`)
- **ZERO Terminal (`APP_ID_TERMINAL`):**
  - Run `help` to list commands.
  - Run `uname -a` to display kernel version and architecture (`ZERO OS 9.0.0 ZERO Ultimate x86_64`).
  - Run `ps` to inspect active processes and priority queues.
  - Run `mem` to inspect PMM/VMM physical and heap allocation statistics.
  - Run `gaming normal|gaming|power` to change system scheduler profiles.
- **ZERO Files (`APP_ID_FILE_MANAGER`):**
  - Explore the native ZEROFS directory tree (`/`, `/boot`, `/system`, `/data`, `/apps`).
- **Control Center (`APP_ID_SETTINGS`):**
  - Six diagnostic tabs: Overview, Display, Audio, Storage, Network, and System Information.
- **ZERO Gaming Control Center (`APP_ID_GAMING_CONTROL`):**
  - Five gaming tabs: Performance Profiles, Low-Latency Scheduler, GPU Capabilities, Audio Ring Buffer, and AI Synchronization.
- **ZERO Strike Arcade (`APP_ID_DEMO_GAME`):**
  - Native 60 FPS arcade space game.
  - **Controls:** `Left/Right` or `A/D` to move ship, `Space` to fire laser, `R` to restart, `ESC` to exit.
  - **HUD:** Real-time top banner displaying frame time (16.6ms), rolling FPS, and active gaming profile.
  - **Automatic Optimization:** Automatically engages `GAMING` scheduler profile (80ms slice) and throttles AI background jobs while playing.
