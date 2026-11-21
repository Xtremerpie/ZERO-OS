# ZERO Software Center (ZERO Store)

**Subsystem:** Graphical Application Store  
**Source:** `kernel/desktop/store.h`, `kernel/desktop/store.c`  
**Dimensions:** 620 x 420 px (Resizable)  
**Parent Subsystems:** ZERO Desktop Compositor, Window Manager, ZERO Package Manager

---

## 1. Overview

The **ZERO Store** is the official graphical app discovery and distribution center in ZERO OS. Integrated directly into the desktop compositor, it provides a unified visual interface for browsing available software, inspecting application details, installing and removing `.zapp` packages, and launching installed native applications.

---

## 2. Interface Layout

The ZERO Store window is organized into four main functional zones:

1. **Header & Category Bar**:
   - Navigation tabs: *All Apps*, *Productivity*, *Development*, *Utilities*, *Internet*, *Media*, and *Installed*.
   - Live Search Input Box: Real-time keyword filtering across application titles and descriptions.
2. **Package Grid Area**:
   - Displays application cards formatted with metallic gold headers and dark void backgrounds.
   - Shows: Title, version badge, category tag, download size, and description summary.
   - Action Button: Dynamically renders **[Install]**, **[Launch]**, or **[Uninstall]** based on package status.
3. **Transaction Status Footer**:
   - Provides live feedback during download, CRC-32 verification, VFS extraction, and uninstallation.

---

## 3. Supported Package Catalog

The default offline repository catalog includes:

| Application | Category | Version | Size | Capabilities |
| :--- | :--- | :--- | :--- | :--- |
| **ZERO Calculator** | Productivity | 1.0.0 | 18 KB | Window, Keypad Math |
| **ZERO Text Editor** | Productivity | 1.0.0 | 24 KB | Window, VFS Storage |
| **ZERO Image Viewer**| Media | 1.0.0 | 28 KB | Window, Zoom Scaling |
| **ZERO Archive Viewer**| Utilities | 1.0.0 | 20 KB | Window, ZAPP Inspection |
| **ZERO Network Monitor**| Utilities | 1.0.0 | 22 KB | Window, BSD Sockets, Telemetry |
| **ZERO Hello World**| Development | 1.0.0 | 16 KB | Window, SDK Template |

---

## 4. Integration with Desktop Shell

- Launchable via application launcher menu (**ZERO OS APPS -> ZERO Store**).
- Accessible via file manager: clicking `.zapp` files invokes direct package inspection.
- Generates desktop toast notifications upon successful package installation or uninstallation.
