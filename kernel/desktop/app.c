/* ZERO KERNEL - Native Application Framework & Registry Implementation (Milestone 6.0.0 ZERO Ecosystem).
 */
#include "desktop/app.h"
#include "desktop/terminal.h"
#include "desktop/file_manager.h"
#include "desktop/control_center.h"
#include "desktop/sysmon.h"
#include "desktop/about.h"
#include "desktop/store.h"
#include "desktop/calc.h"
#include "desktop/editor.h"
#include "desktop/viewer.h"
#include "desktop/archiver.h"
#include "desktop/netmon.h"
#include "desktop/security_center.h"
#include "desktop/ai_assistant.h"
#include "desktop/gaming_control.h"
#include "desktop/demo_game.h"
#include "graphics/font.h"

static app_desc_t s_apps[] = {
    {
        .id = APP_ID_STORE,
        .name = "ZERO Store",
        .icon_glyph = GLYPH_ICON_INFO,
        .launch = store_launch,
        .is_open = store_is_open
    },
    {
        .id = APP_ID_TERMINAL,
        .name = "ZERO Terminal",
        .icon_glyph = GLYPH_ICON_TERMINAL,
        .launch = terminal_launch,
        .is_open = terminal_is_open
    },
    {
        .id = APP_ID_FILE_MANAGER,
        .name = "ZERO Files",
        .icon_glyph = GLYPH_ICON_FOLDER,
        .launch = file_manager_launch,
        .is_open = file_manager_is_open
    },
    {
        .id = APP_ID_EDITOR,
        .name = "Text Editor",
        .icon_glyph = GLYPH_ICON_TERMINAL,
        .launch = editor_launch,
        .is_open = editor_is_open
    },
    {
        .id = APP_ID_CALC,
        .name = "Calculator",
        .icon_glyph = GLYPH_ICON_GEAR,
        .launch = calc_launch,
        .is_open = calc_is_open
    },
    {
        .id = APP_ID_VIEWER,
        .name = "Image Viewer",
        .icon_glyph = GLYPH_ICON_INFO,
        .launch = viewer_launch,
        .is_open = viewer_is_open
    },
    {
        .id = APP_ID_ARCHIVER,
        .name = "Archive Viewer",
        .icon_glyph = GLYPH_ICON_FOLDER,
        .launch = archiver_launch,
        .is_open = archiver_is_open
    },
    {
        .id = APP_ID_NETMON,
        .name = "Network Monitor",
        .icon_glyph = GLYPH_ICON_INFO,
        .launch = netmon_launch,
        .is_open = netmon_is_open
    },
    {
        .id = APP_ID_SECURITY_CENTER,
        .name = "ZERO Security",
        .icon_glyph = GLYPH_ICON_CHECK,
        .launch = security_center_launch,
        .is_open = security_center_is_open
    },
    {
        .id = APP_ID_AI_ASSISTANT,
        .name = "ZERO AI",
        .icon_glyph = GLYPH_ICON_CPU,
        .launch = ai_assistant_launch,
        .is_open = ai_assistant_is_open
    },
    {
        .id = APP_ID_GAMING_CONTROL,
        .name = "Gaming Control",
        .icon_glyph = GLYPH_ICON_GEAR,
        .launch = gaming_control_launch,
        .is_open = gaming_control_is_open
    },
    {
        .id = APP_ID_DEMO_GAME,
        .name = "ZERO Strike",
        .icon_glyph = GLYPH_ICON_CPU,
        .launch = demo_game_launch,
        .is_open = demo_game_is_open
    },
    {
        .id = APP_ID_CONTROL_CENTER,
        .name = "Control Center",
        .icon_glyph = GLYPH_ICON_GEAR,
        .launch = control_center_launch,
        .is_open = control_center_is_open
    },
    {
        .id = APP_ID_SYSTEM_MONITOR,
        .name = "System Monitor",
        .icon_glyph = GLYPH_ICON_CPU,
        .launch = sysmon_launch,
        .is_open = sysmon_is_open
    },
    {
        .id = APP_ID_ABOUT,
        .name = "About ZERO OS",
        .icon_glyph = GLYPH_ICON_CHECK,
        .launch = about_launch,
        .is_open = about_is_open
    }
};

static const uint32_t s_app_count = sizeof(s_apps) / sizeof(s_apps[0]);

void app_init(void)
{
    /* Initialize native application modules */
    store_init();
    calc_init();
    editor_init();
    viewer_init();
    archiver_init();
    netmon_init();
    security_center_init();
    ai_assistant_init();
    gaming_control_init();
    demo_game_init();
}

window_t *app_launch(app_id_t id)
{
    for (uint32_t i = 0; i < s_app_count; i++) {
        if (s_apps[i].id == id && s_apps[i].launch) {
            return s_apps[i].launch();
        }
    }
    return NULL;
}

const app_desc_t *app_get(app_id_t id)
{
    for (uint32_t i = 0; i < s_app_count; i++) {
        if (s_apps[i].id == id) {
            return &s_apps[i];
        }
    }
    return NULL;
}

const app_desc_t *app_get_all(uint32_t *count)
{
    if (count) {
        *count = s_app_count;
    }
    return s_apps;
}
