/* ZERO KERNEL - ZERO File Manager Subsystem (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements native graphical file manager integrated with VFS and ZEROFS:
 * directory browsing, path navigation, file creation, folder creation,
 * file deletion, and file preview inspection.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "desktop/window.h"

#define FM_MAX_ENTRIES  64u

typedef struct fm_entry {
    char     name[64];
    bool     is_dir;
    uint64_t size;
} fm_entry_t;

/* Launch or focus the ZERO File Manager window */
window_t *file_manager_launch(void);

/* Close or query state */
void file_manager_close(void);
bool file_manager_is_open(void);

/* Navigate to specific path */
void file_manager_navigate(const char *path);

/* Create directory, create file, delete file in current folder */
bool file_manager_mkdir(const char *name);
bool file_manager_create_file(const char *name);
bool file_manager_delete(const char *name);

/* Retrieve current directory path */
const char *file_manager_get_path(void);
