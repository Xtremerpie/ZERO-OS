/* ZERO KERNEL - ZERO Gaming Mode Subsystem (Milestone 9.0.0 ZERO Ultimate). */

#include "gaming/gaming.h"
#include "process/scheduler.h"
#include "intelligence/ai_service.h"
#include "security/audit.h"
#include "fs/vfs.h"
#include "zero/log.h"
#include "zero/timer.h"
#include "zero/string.h"

static gaming_profile_t s_current_profile = GAMING_PROFILE_NORMAL;
static gaming_profile_t s_previous_profile = GAMING_PROFILE_NORMAL;

static gaming_config_t s_config = {
    .auto_switch_on_launch = true,
    .pause_ai_background = true,
    .disable_desktop_animations = true,
    .trim_bcache_on_launch = true,
    .target_fps = 60
};

static gaming_telemetry_t s_telemetry = {
    .current_profile = GAMING_PROFILE_NORMAL,
    .active_game_pid = -1,
    .active_game_title = { 0 },
    .game_start_tick = 0,
    .game_total_runtime_ms = 0,
    .current_fps = 60,
    .avg_frame_time_us = 16666,
    .profile_switches_count = 0,
    .ai_throttled = false,
    .hardware_accelerated = true
};

/* Circular buffer for rolling frame time calculation */
#define FRAME_SAMPLES 16u
static uint32_t s_frame_times[FRAME_SAMPLES];
static uint32_t s_frame_idx = 0;
static uint32_t s_frame_count = 0;

void gaming_init(void)
{
    s_current_profile = GAMING_PROFILE_NORMAL;
    s_previous_profile = GAMING_PROFILE_NORMAL;
    memset(&s_telemetry, 0, sizeof(s_telemetry));
    s_telemetry.current_profile = GAMING_PROFILE_NORMAL;
    s_telemetry.active_game_pid = -1;
    s_telemetry.current_fps = 60;
    s_telemetry.avg_frame_time_us = 16666;
    s_telemetry.hardware_accelerated = true;

    /* Load persistent preferences if available */
    gaming_load_config(&s_config);

    KLOG_INFO("gaming", "ZERO Gaming Mode subsystem initialized (Default: NORMAL, Target: %u FPS)",
              s_config.target_fps);
}

bool gaming_set_profile(gaming_profile_t profile)
{
    if (profile == s_current_profile) {
        return true;
    }

    s_previous_profile = s_current_profile;
    s_current_profile = profile;
    s_telemetry.current_profile = profile;
    s_telemetry.profile_switches_count++;

    const char *prof_name = "NORMAL";

    switch (profile) {
        case GAMING_PROFILE_GAMING:
            prof_name = "GAMING";
            scheduler_set_profile(SCHED_PROFILE_LOW_LATENCY);
            s_telemetry.ai_throttled = s_config.pause_ai_background;
            break;

        case GAMING_PROFILE_POWER_SAVER:
            prof_name = "POWER_SAVER";
            scheduler_set_profile(SCHED_PROFILE_POWER_SAVER);
            s_telemetry.ai_throttled = true;
            break;

        case GAMING_PROFILE_NORMAL:
        default:
            prof_name = "NORMAL";
            scheduler_set_profile(SCHED_PROFILE_NORMAL);
            s_telemetry.ai_throttled = false;
            break;
    }

    KLOG_INFO("gaming", "system profile switched to '%s'", prof_name);

    /* Coordinate AI resource consumption with system profile */
    ai_service_set_gaming_mode(profile == GAMING_PROFILE_GAMING);

    /* Audit profile transition for security and administrative compliance */
    audit_log(AUDIT_EVENT_CONFIG_CHANGE, 0, "GAMING_PROFILE", true, prof_name);

    return true;
}

gaming_profile_t gaming_get_profile(void)
{
    return s_current_profile;
}

bool gaming_register_game(pid_t pid, const char *title)
{
    if (pid <= 0) {
        return false;
    }

    s_telemetry.active_game_pid = pid;
    if (title) {
        strncpy(s_telemetry.active_game_title, title, sizeof(s_telemetry.active_game_title) - 1);
        s_telemetry.active_game_title[sizeof(s_telemetry.active_game_title) - 1] = '\0';
    } else {
        strncpy(s_telemetry.active_game_title, "Native Game", sizeof(s_telemetry.active_game_title) - 1);
    }

    s_telemetry.game_start_tick = timer_get_ticks();
    s_telemetry.game_total_runtime_ms = 0;

    /* Set as foreground prioritized game in scheduler */
    scheduler_set_foreground_game(pid);

    KLOG_INFO("gaming", "registered foreground game '%s' (PID: %d)",
              s_telemetry.active_game_title, pid);

    /* Automatically activate gaming profile if configured */
    if (s_config.auto_switch_on_launch) {
        gaming_set_profile(GAMING_PROFILE_GAMING);
    }

    return true;
}

bool gaming_unregister_game(pid_t pid)
{
    if (s_telemetry.active_game_pid != pid) {
        return false;
    }

    uint64_t now_ticks = timer_get_ticks();
    if (now_ticks >= s_telemetry.game_start_tick) {
        /* Timer ticks at 100 Hz (10ms per tick) */
        s_telemetry.game_total_runtime_ms = (now_ticks - s_telemetry.game_start_tick) * 10;
    }

    KLOG_INFO("gaming", "unregistered game '%s' (PID: %d, runtime: %llu ms)",
              s_telemetry.active_game_title, pid,
              (unsigned long long)s_telemetry.game_total_runtime_ms);

    s_telemetry.active_game_pid = -1;
    memset(s_telemetry.active_game_title, 0, sizeof(s_telemetry.active_game_title));
    scheduler_set_foreground_game(-1);

    /* Restore normal profile */
    gaming_set_profile(s_previous_profile);

    return true;
}

void gaming_get_telemetry(gaming_telemetry_t *out_telem)
{
    if (out_telem) {
        *out_telem = s_telemetry;
        if (s_telemetry.active_game_pid > 0) {
            uint64_t now = timer_get_ticks();
            if (now >= s_telemetry.game_start_tick) {
                out_telem->game_total_runtime_ms = (now - s_telemetry.game_start_tick) * 10;
            }
        }
    }
}

void gaming_record_frame(uint32_t frame_time_us)
{
    if (frame_time_us == 0) frame_time_us = 16666;

    s_frame_times[s_frame_idx] = frame_time_us;
    s_frame_idx = (s_frame_idx + 1) % FRAME_SAMPLES;
    if (s_frame_count < FRAME_SAMPLES) {
        s_frame_count++;
    }

    uint32_t sum = 0;
    for (uint32_t i = 0; i < s_frame_count; i++) {
        sum += s_frame_times[i];
    }
    uint32_t avg = sum / s_frame_count;
    s_telemetry.avg_frame_time_us = avg;
    if (avg > 0) {
        s_telemetry.current_fps = 1000000u / avg;
    }
}

bool gaming_load_config(gaming_config_t *out_cfg)
{
    if (!out_cfg) return false;
    file_t *file = vfs_open(GAMING_CONFIG_PATH, O_RDONLY);
    if (!file) { *out_cfg = s_config; return false; }
    char buf[128];
    int64_t bytes = vfs_read(file, buf, sizeof(buf) - 1);
    vfs_close(file);
    if (bytes <= 0) { *out_cfg = s_config; return false; }
    buf[(size_t)bytes < sizeof(buf) ? (size_t)bytes : sizeof(buf) - 1] = '\0';
    if (strstr(buf, "auto_switch=0")) s_config.auto_switch_on_launch = false;
    if (strstr(buf, "pause_ai=0")) s_config.pause_ai_background = false;
    if (strstr(buf, "target_fps=30")) s_config.target_fps = 30;
    if (strstr(buf, "target_fps=120")) s_config.target_fps = 120;
    *out_cfg = s_config;
    return true;
}

bool gaming_save_config(const gaming_config_t *cfg)
{
    if (!cfg) return false;
    s_config = *cfg;
    char buf[128];
    size_t pos = 0;
    const char *a = s_config.auto_switch_on_launch ? "auto_switch=1\n" : "auto_switch=0\n";
    const char *b = s_config.pause_ai_background ? "pause_ai=1\n" : "pause_ai=0\n";
    while (*a && pos + 1 < sizeof(buf)) buf[pos++] = *a++;
    while (*b && pos + 1 < sizeof(buf)) buf[pos++] = *b++;
    const char *fps = "target_fps=60\n";
    while (*fps && pos + 1 < sizeof(buf)) buf[pos++] = *fps++;
    file_t *file = vfs_open(GAMING_CONFIG_PATH, O_WRONLY | O_CREAT | O_TRUNC);
    if (!file) return false;
    int64_t written = vfs_write(file, buf, pos);
    vfs_close(file);
    return written == (int64_t)pos;
}
