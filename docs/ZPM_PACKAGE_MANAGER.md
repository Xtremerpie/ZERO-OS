# ZERO Package Manager (zpm) Reference

**Subsystem:** ZERO Package Manager  
**Kernel Headers:** `kernel/apps/zpm.h`, `kernel/apps/zpm.c`  
**Host Tool:** `tools/zapp_tool.py`  
**Storage Path:** `/data/apps/<app_id>/`, `/data/pkg/installed.db`

---

## 1. Overview

The ZERO Package Manager (`zpm`) is the official package management subsystem of ZERO OS. It handles package installation, uninstallation, verification, local database management, and remote repository queries.

`zpm` can be accessed both programmatically via C kernel APIs and interactively via the ZERO Terminal command line interface or the graphical ZERO Store.

---

## 2. CLI Command Reference

`zpm` commands can be entered directly into ZERO Terminal:

```sh
# List all currently installed applications
zero@os$ zpm list

# Display detailed metadata for an application
zero@os$ zpm info calc

# Install a .zapp package archive from filesystem
zero@os$ zpm install /data/calc-1.0.0.zapp

# Remove an installed application
zero@os$ zpm uninstall calc

# Search package repository by keyword or category
zero@os$ zpm search editor

# Verify integrity of installed package on disk
zero@os$ zpm verify calc

# Synchronize local repository index
zero@os$ zpm update
```

---

## 3. Kernel API Reference

### 3.1 Subsystem Initialization
```c
void zpm_init(void);
```
Initializes package manager database, mounts storage paths `/data/apps` and `/data/pkg`, and loads registered package records.

### 3.2 Query Functions
```c
int32_t zpm_list(zpm_record_t *out_records, uint32_t max_records);
int32_t zpm_get_installed_count(void);
bool    zpm_info(const char *app_id, zpm_record_t *out_record);
bool    zpm_is_installed(const char *app_id);
int32_t zpm_search(const char *query, zpm_search_result_t *out_results, uint32_t max_results);
```

### 3.3 Transactional Operations
```c
zapp_err_t zpm_install_buffer(const uint8_t *pkg_data, size_t pkg_size);
zapp_err_t zpm_install_file(const char *file_path);
bool       zpm_uninstall(const char *app_id);
bool       zpm_verify(const char *app_id);
```

---

## 4. Packaging and Build Tools

A host-side Python utility (`tools/zapp_tool.py`) is provided for developers to build and inspect packages on host systems:

```bash
# Build a .zapp package from a manifest and directory
python tools/zapp_tool.py create --manifest manifest.json --dir app_files/ --out myapp.zapp

# Validate package integrity and security
python tools/zapp_tool.py validate myapp.zapp

# Inspect header and file entries
python tools/zapp_tool.py info myapp.zapp

# Extract package contents
python tools/zapp_tool.py extract myapp.zapp --out destination_dir/
```
