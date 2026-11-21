/* ZERO KERNEL - ZERO Gaming Mode Subsystem (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements dedicated system performance profiles (NORMAL, GAMING, POWER_SAVER),
 * foreground game process prioritization, background task coordination,
 * dynamic profile switching, audit logging, and gaming telemetry.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "process/process.h"

#define GAMING_CONFIG_PATH "/data/config/gaming.cfg"
#define GAMING_MAX_GAME_TITLE 32u

typedef enum {
    GAMING_PROFILE_NORMAL = 0,       /* Balanced system configuration */
    GAMING_PROFILE_GAMING,           /* Low-latency scheduler, prioritized foreground, throttled AI */
    GAMING_PROFILE_POWER_SAVER       /* Energy saving, coalesced wakeups, lower switch rate */
} gaming_profile_t;

typedef struct gaming_config {
    bool     auto_switch_on_launch;
    bool     pause_ai_background;
    bool     disable_desktop_animations;
    bool     trim_bcache_on_launch;
    uint32_t target_fps;             /* e.g. 60 FPS */
} gaming_config_t;

typedef struct gaming_telemetry {
    gaming_profile_t current_profile;
    pid_t            active_game_pid;
    char             active_game_title[GAMING_MAX_GAME_TITLE];
    uint64_t         game_start_tick;
    uint64_t         game_total_runtime_ms;
    uint32_t         current_fps;
    uint32_t         avg_frame_time_us;
    uint32_t         profile_switches_count;
    bool             ai_throttled;
    bool             hardware_accelerated;
} gaming_telemetry_t;

/* Initialize gaming mode subsystem and load configuration */
void gaming_init(void);

/* Switch active system profile */
bool gaming_set_profile(gaming_profile_t profile);

/* Query active system profile */
gaming_profile_t gaming_get_profile(void);

/* Register a running game process for prioritization and auto profile activation */
bool gaming_register_game(pid_t pid, const char *title);

/* Unregister a game process and automatically restore previous profile */
bool gaming_unregister_game(pid_t pid);

/* Query gaming telemetry and metrics */
void gaming_get_telemetry(gaming_telemetry_t *out_telem);

/* Record frame render duration for FPS calculation */
void gaming_record_frame(uint32_t frame_time_us);

/* Load and save persistent gaming configuration */
bool gaming_load_config(gaming_config_t *out_cfg);
bool gaming_save_config(const gaming_config_t *cfg);
