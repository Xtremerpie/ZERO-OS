/* ZERO KERNEL - ZERO Package Manager (zpm) Subsystem (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements package installation, removal, transactional rollback,
 * installed application database, verification, search, and CLI interface.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "apps/zapp.h"

#define ZPM_MAX_INSTALLED  32u
#define ZPM_MAX_REPO_PKGS  64u

typedef struct zpm_record {
    char     app_id[32];
    char     version[16];
    char     name[64];
    char     description[128];
    char     category[32];
    char     install_path[128];
    char     exec_path[128];
    uint8_t  icon_glyph;
    uint32_t installed_size;
    uint32_t permissions;
    bool     active;
} zpm_record_t;

typedef struct zpm_search_result {
    char     app_id[32];
    char     version[16];
    char     name[64];
    char     category[32];
    char     description[128];
    uint32_t package_size;
    bool     installed;
} zpm_search_result_t;

/* Initialize package manager and load installed packages */
void zpm_init(void);

/* List currently installed applications */
int32_t zpm_list(zpm_record_t *out_records, uint32_t max_records);
int32_t zpm_get_installed_count(void);

/* Query detailed metadata for a package */
bool zpm_info(const char *app_id, zpm_record_t *out_record);

/* Check if an application is installed */
bool zpm_is_installed(const char *app_id);

/* Install a package from memory buffer */
zapp_err_t zpm_install_buffer(const uint8_t *pkg_data, size_t pkg_size);

/* Install a package from a .zapp file on filesystem */
zapp_err_t zpm_install_file(const char *file_path);

/* Uninstall an application */
bool zpm_uninstall(const char *app_id);

/* Verify package files and integrity on disk */
bool zpm_verify(const char *app_id);

/* Cryptographic signature enforcement and verification (Milestone 7.0.0 ZERO Security) */
bool zpm_is_signature_enforced(void);
void zpm_set_signature_enforced(bool enforced);
bool zpm_verify_package_signature(const uint8_t *pkg_data, size_t pkg_size);

/* Search local/remote package catalog */
int32_t zpm_search(const char *query, zpm_search_result_t *out_results, uint32_t max_results);

/* Execute zpm CLI command (for terminal and scripting) */
int32_t zpm_exec_cli(const char *cmd_line, char *out_buf, size_t out_max);
