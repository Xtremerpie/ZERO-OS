# ZERO Desktop User & Environment Guide (Milestone 5.0.0)

**Codename:** ZERO Desktop  
**Status:** COMPLETE  
**Visual Style:** Deep Void Black, Dark Graphite, Metallic Gold, Crimson Red  

---

## 1. Visual Overview & Desktop Paradigm

ZERO Desktop is designed from scratch as a futuristic, clean, and distraction-free workstation environment. It eschews heavy blur shaders, bulky rounded corners, and desktop bloat in favor of sharp geometry, instant responsiveness, and high-contrast readability.

---

## 2. Desktop Elements

### 2.1 Graphical Login Screen
When ZERO OS boots, it presents the official Graphical Login Screen:
- Center card framed with a metallic gold border and header badge.
- Pre-filled username field: `zero`.
- Masked password field.
- "SIGN IN" action button.
- **Honest Development Authentication Mode Notice:**  
  `[ NOTICE: Development Authentication Mode - Press Sign In or Enter ]`  
  In dev mode, pressing Enter or clicking "Sign In" unlocks the desktop session immediately.

### 2.2 Desktop Workspace
Upon login, the desktop wallpaper appears:
- Background: Deep Void Black (`#08080A`).
- Geometric graphite lattice grid with gold corner accent brackets.
- Central watermark branding: "ZERO OS - Beyond Traditional Computing".

### 2.3 System Taskbar (Height 36px)
Located along the bottom edge of the screen:
- **Start Button ("ZERO"):** Left-anchored button at `x = 6`. Clicking it (or pressing the Super/Win key) opens the Application Launcher menu.
- **Window Tabs:** Open and minimized windows appear as taskbar tabs. Clicking an inactive tab brings that window to the front and focuses it; clicking the active tab minimizes the window. The currently focused window displays an active gold underline marker.
- **System Tray (Right Side):**
  * Network Status Indicator (`[NET]` icon): Green when TCP/IP and e1000 controller are active.
  * Storage Activity Indicator (`[DSK]` icon): Gold when filesystem transactions are occurring.
  * CPU Load Indicator (`[CPU]` icon): Real-time processor telemetry.
  * System Clock / Uptime counter.

### 2.4 Application Launcher Menu
A sleek popup menu positioned above the Start button:
- **Header:** "ZERO OS APPS" in metallic gold.
- **Entries:**
  1. `[>_]` **ZERO Terminal:** Interactive command-line environment.
  2. `[/]` **ZERO Files:** Native graphical file manager.
  3. `[*]` **Control Center:** Hardware and system settings.
  4. `[%]` **System Monitor:** Real-time resource utilization and process table.
  5. `[i]` **About ZERO OS:** Kernel architectural specifications and mission.

---

## 3. Global Keyboard Shortcuts

| Shortcut | Description |
| :--- | :--- |
| **Super / Win Key** | Toggle Application Launcher popup menu |
| **Escape** | Dismiss Application Launcher menu |
| **Alt + Tab / F6** | Cycle active window focus forward across the window stack |
| **Alt + F4** | Close the currently focused window |

---

## 4. Native Applications

### 4.1 ZERO Terminal
- Full interactive shell operating inside a resizable graphical window.
- Visual styling: Void Black background with gold prompt (`zero@os$ `) and crisp white output.
- Real kernel commands:
  * `help`: Overview of available commands.
  * `version`: Kernel build and milestone release version.
  * `memory`: PMM and VMM memory allocation status.
  * `cpu`: Processor architecture and APIC timer frequencies.
  * `devices`: PCI and peripheral inventory.
  * `storage`: Partition tables, block devices, and ZEROFS mounts.
  * `network`: Network controllers, MAC, IP, and socket status.
  * `processes`: Preemptive scheduler process table.
  * `uptime`: Kernel ticks and uptime duration.
  * `pwd`: Print current working directory.
  * `cd <path>`: Change working directory.
  * `ls`: List directory contents.
  * `cat <filename>`: View file contents via VFS.
  * `clear`: Clear console scrollback buffer.

### 4.2 ZERO Files
- Native file manager directly interacting with VFS and ZEROFS storage.
- Breadcrumb path bar displaying current folder (`/`, `/bin`, `/data`, etc.).
- "UP" button to navigate to parent directories.
- Places sidebar: Instant navigation to `/` (RAMFS Root), `/data` (ZEROFS Persistent Partition), and `/dev` (Hardware Devices).
- File operations:
  * Create new folders (`+Folder` button calling `vfs_mkdir`).
  * Create new files (`+File` button calling `vfs_create`).
  * Delete files (`Del` button calling `vfs_unlink`).
  * Preview pane: Reading and displaying file contents when selected.

### 4.3 Control Center
- Central system settings with 6 interactive sidebar tabs:
  1. **Display:** Resolution (1024x768 @ 32 bpp), tear-free double buffering, damage bounding-box tracking, and clipping enforcement.
  2. **Input:** PS/2 mouse (100Hz, 9-bit deltas), PS/2 keyboard (Set 2 dual scancode decoder), and flicker-free cursor overlay.
  3. **System:** CPU model, cores, threads, total RAM (4096 MB), PMM/VMM paging model with W^X protection.
  4. **Network:** Intel 82540EM (e1000) Gigabit controller, MAC address, IPv4 address (10.0.2.15), and BSD socket API.
  5. **Storage:** VirtIO / RAM disk block controllers, GPT and legacy MBR tables, ZEROFS v1 filesystem (4096B block cache, 128B inodes).
  6. **Themes:** Dark Abyss official color palette details and styling tokens.

### 4.4 System Monitor
- Real-time CPU usage meter bar.
- RAM physical memory allocation breakdown bar.
- Active process table displaying PID, Name, State, Memory footprint, and CPU percentage.
- Storage disk I/O and network packet throughput counters.

### 4.5 About ZERO OS
- System branding and emblem.
- Tagline: *"Beyond Traditional Computing."*
- Full kernel architectural summary and philosophy notice.
