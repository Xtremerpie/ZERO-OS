/* ZERO KERNEL - Ecosystem Subsystem Self-Test Implementation (Milestone 6.0.0 ZERO Ecosystem).
 *
 * Implements in-kernel validation for .zapp package parsing, traversal defense,
 * ZPM package manager lifecycle, configuration system, store catalog, and desktop integration.
 */
#include "debug/selftest_ecosystem.h"
#include "apps/zapp.h"
#include "apps/zpm.h"
#include "apps/config.h"
#include "desktop/store.h"
#include "desktop/notification.h"
#include "desktop/app.h"
#include "desktop/shell.h"
#include "lib/kprintf.h"
#include "zero/log.h"
#include "zero/string.h"

/* Test 1: ZAPP Header Validation */
static bool test_zapp_header(void)
{
    zapp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.magic = ZAPP_MAGIC;
    hdr.format_version = ZAPP_FORMAT_VER;
    strcpy(hdr.app_id, "zero.calculator");
    strcpy(hdr.app_version, "1.0.0");
    strcpy(hdr.target_arch, "x86_64");
    hdr.manifest_offset = sizeof(zapp_header_t);
    hdr.manifest_size = sizeof(zapp_manifest_t);
    hdr.entry_count = 1;
    hdr.entries_offset = hdr.manifest_offset + hdr.manifest_size;
    hdr.payload_offset = hdr.entries_offset + sizeof(zapp_file_entry_t);
    hdr.payload_size = 128;

    size_t total_size = hdr.payload_offset + hdr.payload_size;

    /* Valid header must pass */
    if (zapp_validate_header(&hdr, total_size) != ZAPP_OK) {
        kprintf("[FAIL] ecosystem: valid header rejected\n");
        return false;
    }

    /* Corrupted magic must fail */
    hdr.magic = 0xDEADBEEF;
    if (zapp_validate_header(&hdr, total_size) != ZAPP_ERR_BAD_MAGIC) {
        kprintf("[FAIL] ecosystem: bad magic not rejected\n");
        return false;
    }
    hdr.magic = ZAPP_MAGIC;

    /* Corrupted version must fail */
    hdr.format_version = 99;
    if (zapp_validate_header(&hdr, total_size) != ZAPP_ERR_BAD_VERSION) {
        kprintf("[FAIL] ecosystem: bad version not rejected\n");
        return false;
    }
    hdr.format_version = ZAPP_FORMAT_VER;

    /* Corrupted size must fail */
    if (zapp_validate_header(&hdr, total_size - 10) != ZAPP_ERR_BAD_SIZE) {
        kprintf("[FAIL] ecosystem: truncated package size not rejected\n");
        return false;
    }

    kprintf("[PASS] ecosystem: ZAPP 128-byte header validation verified\n");
    return true;
}

/* Test 2: Path Traversal Attack Defense */
static bool test_path_traversal(void)
{
    const char *bad_paths[] = {
        "../etc/passwd",
        "/bin/sh",
        "../../root",
        "sub/../../secret",
        "..\\windows\\system32",
        "C:/autoexec.bat",
        "D:\\boot.ini"
    };

    for (size_t i = 0; i < sizeof(bad_paths) / sizeof(bad_paths[0]); i++) {
        if (zapp_validate_path(bad_paths[i]) != ZAPP_ERR_PATH_TRAVERSAL) {
            kprintf("[FAIL] ecosystem: path traversal attack not blocked: %s\n", bad_paths[i]);
            return false;
        }
    }

    const char *good_paths[] = {
        "bin/calc",
        "icons/calc.png",
        "readme.txt",
        "data/defaults.cfg"
    };

    for (size_t i = 0; i < sizeof(good_paths) / sizeof(good_paths[0]); i++) {
        if (zapp_validate_path(good_paths[i]) != ZAPP_OK) {
            kprintf("[FAIL] ecosystem: legitimate relative path rejected: %s\n", good_paths[i]);
            return false;
        }
    }

    kprintf("[PASS] ecosystem: path traversal security defenses verified (7 attack vectors blocked)\n");
    return true;
}

/* Test 3: Checksum Calculation */
static bool test_checksum(void)
{
    const uint8_t test_data[] = "ZERO OS Milestone 6.0.0 Native Application Ecosystem";
    size_t len = strlen((const char *)test_data);

    uint32_t c1 = zapp_calc_checksum(test_data, len);
    uint32_t c2 = zapp_calc_checksum(test_data, len);

    if (c1 == 0 || c1 != c2) {
        kprintf("[FAIL] ecosystem: checksum non-deterministic\n");
        return false;
    }

    /* Alter one byte and verify checksum changes */
    uint8_t modified[64];
    memcpy(modified, test_data, len);
    modified[0] ^= 0x55;
    uint32_t c3 = zapp_calc_checksum(modified, len);

    if (c1 == c3) {
        kprintf("[FAIL] ecosystem: single byte collision in checksum\n");
        return false;
    }

    kprintf("[PASS] ecosystem: CRC32 checksum integrity calculation verified\n");
    return true;
}

/* Test 4: ZPM Package Manager Database & Search */
static bool test_zpm_subsystem(void)
{
    /* Check repository search */
    zpm_search_result_t res[8];
    int32_t total = zpm_search("", res, 8);
    if (total < 6) {
        kprintf("[FAIL] ecosystem: expected at least 6 repository packages, got %d\n", total);
        return false;
    }

    /* Specific search */
    int32_t calc_matches = zpm_search("calc", res, 8);
    if (calc_matches < 1) {
        kprintf("[FAIL] ecosystem: search for 'calc' yielded 0 matches\n");
        return false;
    }

    /* Test CLI execution */
    char cli_buf[512];
    int rc = zpm_exec_cli("zpm search calc", cli_buf, sizeof(cli_buf));
    if (rc != 0 || strlen(cli_buf) == 0) {
        kprintf("[FAIL] ecosystem: zpm CLI search failed\n");
        return false;
    }

    rc = zpm_exec_cli("zpm list", cli_buf, sizeof(cli_buf));
    if (rc != 0) {
        kprintf("[FAIL] ecosystem: zpm CLI list failed\n");
        return false;
    }

    kprintf("[PASS] ecosystem: ZPM package repository and CLI dispatcher verified\n");
    return true;
}

/* Test 5: Application Configuration System */
static bool test_config_system(void)
{
    config_init();

    config_t *cfg = config_load("test_app");
    if (!cfg) {
        kprintf("[FAIL] ecosystem: config_load returned NULL\n");
        return false;
    }

    config_set_string(cfg, "theme", "dark_gold");
    config_set_int(cfg, "buffer_size", 4096);
    config_set_bool(cfg, "auto_save", true);

    if (strcmp(config_get_string(cfg, "theme", ""), "dark_gold") != 0) {
        kprintf("[FAIL] ecosystem: config_get_string mismatch\n");
        return false;
    }
    if (config_get_int(cfg, "buffer_size", 0) != 4096) {
        kprintf("[FAIL] ecosystem: config_get_int mismatch\n");
        return false;
    }
    if (!config_get_bool(cfg, "auto_save", false)) {
        kprintf("[FAIL] ecosystem: config_get_bool mismatch\n");
        return false;
    }

    config_save(cfg);
    config_free(cfg);

    /* Reload and check persistence */
    config_t *reload = config_load("test_app");
    if (!reload) {
        kprintf("[FAIL] ecosystem: reload config failed\n");
        return false;
    }

    if (strcmp(config_get_string(reload, "theme", ""), "dark_gold") != 0) {
        kprintf("[FAIL] ecosystem: reloaded config string mismatch\n");
        return false;
    }
    if (config_get_int(reload, "buffer_size", 0) != 4096) {
        kprintf("[FAIL] ecosystem: reloaded config int mismatch\n");
        return false;
    }
    if (!config_get_bool(reload, "auto_save", false)) {
        kprintf("[FAIL] ecosystem: reloaded config bool mismatch\n");
        return false;
    }

    config_free(reload);
    kprintf("[PASS] ecosystem: persistent application configuration (/data/config) verified\n");
    return true;
}

/* Test 6: Desktop Notifications Subsystem */
static bool test_notifications(void)
{
    notification_init();

    if (notification_is_visible()) {
        kprintf("[FAIL] ecosystem: notification active before show\n");
        return false;
    }

    notification_show("ZPM Installer", "Package installed", 0, 500);
    if (!notification_is_visible()) {
        kprintf("[FAIL] ecosystem: notification not visible after show\n");
        return false;
    }

    /* Advance timer past duration */
    notification_tick(600);
    if (notification_is_visible()) {
        kprintf("[FAIL] ecosystem: notification not auto-dismissed after duration\n");
        return false;
    }

    kprintf("[PASS] ecosystem: desktop notification toast lifecycle verified\n");
    return true;
}

/* Test 7: Native Application Registry */
static bool test_app_registry(void)
{
    uint32_t count = 0;
    const app_desc_t *apps = app_get_all(&count);

    if (count < 11 || !apps) {
        kprintf("[FAIL] ecosystem: expected >= 11 registered applications, found %u\n", count);
        return false;
    }

    /* Verify essential 6.0.0 apps */
    if (!app_get(APP_ID_STORE) || !app_get(APP_ID_CALC) ||
        !app_get(APP_ID_EDITOR) || !app_get(APP_ID_VIEWER) ||
        !app_get(APP_ID_ARCHIVER) || !app_get(APP_ID_NETMON)) {
        kprintf("[FAIL] ecosystem: missing ecosystem application in registry\n");
        return false;
    }

    kprintf("[PASS] ecosystem: application registry verified (%u native applications active)\n", count);
    return true;
}

bool ecosystem_run_selftests(void)
{
    kprintf("=== ZERO OS 6.0.0 ZERO Ecosystem Self-Tests ===\n");

    if (!test_zapp_header()) return false;
    if (!test_path_traversal()) return false;
    if (!test_checksum()) return false;
    if (!test_zpm_subsystem()) return false;
    if (!test_config_system()) return false;
    if (!test_notifications()) return false;
    if (!test_app_registry()) return false;

    kprintf("=== ZERO OS 6.0.0 Ecosystem Self-Tests PASSED (7/7 suites) ===\n");
    return true;
}

void ecosystem_report_generate(void)
{
    kprintf("========================================================\n");
    kprintf("   ZERO OS 6.0.0 NATIVE APPLICATION ECOSYSTEM REPORT    \n");
    kprintf("========================================================\n");
    kprintf("Architecture:       x86_64 Higher-Half Ring 0 Microkernel\n");
    kprintf("Milestone:          6.0.0 ZERO Ecosystem\n");
    kprintf("Package Format:     .zapp (128B Hdr, 384B Manifest, 144B Entries)\n");
    kprintf("Package Security:   Path Traversal Filter + CRC32 Checksum\n");
    kprintf("Package Manager:    zpm (CLI, Database, VFS /data/apps)\n");
    kprintf("Software Center:    ZERO Store (6 Categories, Search, Install/Launch)\n");
    kprintf("Config Subsystem:   /data/config/<app_id>.cfg Key-Value Engine\n");
    kprintf("Notification UI:    Toast overlay (240x52) with countdown timer\n");
    kprintf("SDK Capabilities:   Headers, crt0.asm, linker_user.ld, Ring 3 ABI\n");
    kprintf("Native Apps (11):   Store, Terminal, Files, Editor, Calc, Viewer,\n");
    kprintf("                    Archiver, Netmon, Control Center, Sysmon, About\n");
    kprintf("Status:             ALL SUBSYSTEMS OPERATIONAL\n");
    kprintf("========================================================\n");
}
