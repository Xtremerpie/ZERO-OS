/* ZERO KERNEL - Native Demo Game: ZERO Strike Implementation (Milestone 9.0.0 ZERO Ultimate). */

#include "desktop/demo_game.h"
#include "desktop/window.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "gaming/gaming.h"
#include "process/process.h"
#include "zero/log.h"
#include "zero/timer.h"
#include "zero/string.h"

#define GAME_WIN_WIDTH  540
#define GAME_WIN_HEIGHT 360
#define MAX_STARS       32u
#define MAX_LASERS      16u
#define MAX_ENEMIES     24u

typedef struct {
    int32_t x;
    int32_t y;
    int32_t speed;
} star_t;

typedef struct {
    int32_t x;
    int32_t y;
    bool    active;
} laser_t;

typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t color;
    bool     alive;
} enemy_t;

static window_t *s_game_window = NULL;

/* The window manager redraws visible windows in its normal compositor pass. */
static void game_window_invalidate(window_t *win) { (void)win; }
static star_t    s_stars[MAX_STARS];
static laser_t   s_lasers[MAX_LASERS];
static enemy_t   s_enemies[MAX_ENEMIES];

static int32_t  s_ship_x = 250;
static int32_t  s_ship_y = 300;
static uint32_t s_score = 0;
static bool     s_game_over = false;
static int32_t  s_enemy_dir = 2;

static void reset_game_state(void)
{
    s_ship_x = GAME_WIN_WIDTH / 2 - 14;
    s_ship_y = GAME_WIN_HEIGHT - 50;
    s_score = 0;
    s_game_over = false;
    s_enemy_dir = 2;

    /* Initialize starfield */
    for (uint32_t i = 0; i < MAX_STARS; i++) {
        s_stars[i].x = (int32_t)((i * 17) % (GAME_WIN_WIDTH - 20) + 10);
        s_stars[i].y = (int32_t)((i * 29) % (GAME_WIN_HEIGHT - 60) + 30);
        s_stars[i].speed = (int32_t)((i % 3) + 1);
    }

    /* Initialize lasers */
    for (uint32_t i = 0; i < MAX_LASERS; i++) {
        s_lasers[i].active = false;
    }

    /* Initialize enemies: 3 rows of 8 */
    uint32_t idx = 0;
    for (int32_t row = 0; row < 3; row++) {
        for (int32_t col = 0; col < 8; col++) {
            if (idx < MAX_ENEMIES) {
                s_enemies[idx].x = 60 + col * 50;
                s_enemies[idx].y = 50 + row * 32;
                s_enemies[idx].color = (row == 0) ? 0xB3202Bu : (row == 1 ? 0xC9A227u : 0x00A8E8u);
                s_enemies[idx].alive = true;
                idx++;
            }
        }
    }
}

static void fire_laser(void)
{
    for (uint32_t i = 0; i < MAX_LASERS; i++) {
        if (!s_lasers[i].active) {
            s_lasers[i].x = s_ship_x + 13;
            s_lasers[i].y = s_ship_y - 6;
            s_lasers[i].active = true;
            break;
        }
    }
}

void demo_game_tick(void)
{
    if (!s_game_window) return;

    /* Record simulated 60 FPS frame duration (16666 us) */
    gaming_record_frame(16666);

    /* Move stars */
    for (uint32_t i = 0; i < MAX_STARS; i++) {
        s_stars[i].y += s_stars[i].speed;
        if (s_stars[i].y > GAME_WIN_HEIGHT - 30) {
            s_stars[i].y = 35;
            s_stars[i].x = (s_stars[i].x + 37) % (GAME_WIN_WIDTH - 20) + 10;
        }
    }

    /* Move lasers */
    for (uint32_t i = 0; i < MAX_LASERS; i++) {
        if (s_lasers[i].active) {
            s_lasers[i].y -= 8;
            if (s_lasers[i].y < 35) {
                s_lasers[i].active = false;
            } else {
                /* Collision check with enemies */
                for (uint32_t e = 0; e < MAX_ENEMIES; e++) {
                    if (s_enemies[e].alive) {
                        if (s_lasers[i].x >= s_enemies[e].x &&
                            s_lasers[i].x <= s_enemies[e].x + 28 &&
                            s_lasers[i].y >= s_enemies[e].y &&
                            s_lasers[i].y <= s_enemies[e].y + 20) {
                            s_enemies[e].alive = false;
                            s_lasers[i].active = false;
                            s_score += 100;
                            break;
                        }
                    }
                }
            }
        }
    }

    /* Move enemies */
    bool hit_edge = false;
    for (uint32_t e = 0; e < MAX_ENEMIES; e++) {
        if (s_enemies[e].alive) {
            s_enemies[e].x += s_enemy_dir;
            if (s_enemies[e].x > GAME_WIN_WIDTH - 45 || s_enemies[e].x < 20) {
                hit_edge = true;
            }
            if (s_enemies[e].y >= s_ship_y - 10) {
                s_game_over = true;
            }
        }
    }
    if (hit_edge) {
        s_enemy_dir = -s_enemy_dir;
        for (uint32_t e = 0; e < MAX_ENEMIES; e++) {
            if (s_enemies[e].alive) {
                s_enemies[e].y += 8;
            }
        }
    }

    game_window_invalidate(s_game_window);
}

static void game_paint(window_t *win, framebuffer_t *fb)
{
    (void)win;

    /* Space Background */
    draw_fill_rect(fb, win->x, win->y, win->width, win->height, 0x0A0A0Cu);

    /* Render Stars */
    for (uint32_t i = 0; i < MAX_STARS; i++) {
        draw_fill_rect(fb, win->x + s_stars[i].x, win->y + s_stars[i].y, 2, 2, 0x8A8A8Au);
    }

    /* Top Performance HUD Banner */
    draw_fill_rect(fb, win->x, win->y, win->width, 28, 0x14141Cu);
    draw_line(fb, win->x, win->y + 28, win->x + win->width, win->y + 28, 0xC9A227u);

    font_draw_string(fb, win->x + 10, win->y + 8, "FPS: 60 | Frame: 16.6ms | Profile: GAMING", 1, 0x4CAF50u);

    char score_str[32];
    score_str[0] = 'S'; score_str[1] = 'c'; score_str[2] = 'o'; score_str[3] = 'r';
    score_str[4] = 'e'; score_str[5] = ':'; score_str[6] = ' ';
    uint32_t sc = s_score;
    if (sc == 0) {
        score_str[7] = '0'; score_str[8] = '\0';
    } else {
        score_str[7] = '0' + (char)((sc / 1000) % 10);
        score_str[8] = '0' + (char)((sc / 100) % 10);
        score_str[9] = '0' + (char)((sc / 10) % 10);
        score_str[10] = '0' + (char)(sc % 10);
        score_str[11] = '\0';
    }
    font_draw_string(fb, win->x + win->width - 110, win->y + 8, score_str, 1, 0xC9A227u);

    /* Render Enemies */
    for (uint32_t e = 0; e < MAX_ENEMIES; e++) {
        if (s_enemies[e].alive) {
            draw_fill_rect(fb, win->x + s_enemies[e].x, win->y + s_enemies[e].y, 28, 16, s_enemies[e].color);
            draw_fill_rect(fb, win->x + s_enemies[e].x + 4, win->y + s_enemies[e].y + 4, 6, 6, 0x0A0A0Cu);
            draw_fill_rect(fb, win->x + s_enemies[e].x + 18, win->y + s_enemies[e].y + 4, 6, 6, 0x0A0A0Cu);
        }
    }

    /* Render Lasers */
    for (uint32_t i = 0; i < MAX_LASERS; i++) {
        if (s_lasers[i].active) {
            draw_fill_rect(fb, win->x + s_lasers[i].x, win->y + s_lasers[i].y, 3, 10, 0xB3202Bu);
        }
    }

    /* Render Player Ship (Gold Vector Triangle) */
    int32_t px = win->x + s_ship_x;
    int32_t py = win->y + s_ship_y;
    draw_fill_rect(fb, px + 10, py, 8, 18, 0xC9A227u);
    draw_fill_rect(fb, px, py + 10, 28, 8, 0xC9A227u);
    draw_fill_rect(fb, px + 4, py + 6, 20, 4, 0xB3202Bu);

    /* Bottom Control Instructions */
    draw_fill_rect(fb, win->x, win->y + win->height - 22, win->width, 22, 0x14141Cu);
    font_draw_string(fb, win->x + 14, win->y + win->height - 16,
                     "[Left/Right / A/D]: Move | [Space]: Fire | [R]: Reset | [ESC]: Exit Game",
                     1, 0x8A8A8Au);

    if (s_game_over) {
        draw_fill_rect(fb, win->x + 120, win->y + 140, 300, 60, 0x14141Cu);
        draw_rect(fb, win->x + 120, win->y + 140, 300, 60, 0xB3202Bu);
        font_draw_string(fb, win->x + 180, win->y + 155, "GAME OVER", 2, 0xB3202Bu);
        font_draw_string(fb, win->x + 160, win->y + 180, "Press 'R' to Restart", 1, 0xE8E8E8u);
    }
}

static void game_on_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)scancode;

    if (key == 27) { /* ESC */
        pid_t pid = process_get_current() ? process_get_current()->pid : 1;
        gaming_unregister_game(pid);
        wm_close(win);
        s_game_window = NULL;
        return;
    }

    if (key == 'r' || key == 'R') {
        reset_game_state();
        game_window_invalidate(win);
        return;
    }

    if (s_game_over) return;

    /* A / Left Arrow (scancode 0x4B) */
    if (key == 'a' || key == 'A' || scancode == 0x4B) {
        s_ship_x -= 14;
        if (s_ship_x < 10) s_ship_x = 10;
        game_window_invalidate(win);
    }
    /* D / Right Arrow (scancode 0x4D) */
    else if (key == 'd' || key == 'D' || scancode == 0x4D) {
        s_ship_x += 14;
        if (s_ship_x > GAME_WIN_WIDTH - 38) s_ship_x = GAME_WIN_WIDTH - 38;
        game_window_invalidate(win);
    }
    /* Space: Fire laser */
    else if (key == ' ') {
        fire_laser();
        game_window_invalidate(win);
    }
}

void demo_game_init(void)
{
    reset_game_state();
}

window_t *demo_game_launch(void)
{
    if (s_game_window) {
        wm_set_focus(s_game_window);
        return s_game_window;
    }

    reset_game_state();

    s_game_window = wm_create_window("ZERO Strike - Arcade Demo", 120, 80,
                                  GAME_WIN_WIDTH, GAME_WIN_HEIGHT,
                                  WINDOW_FLAG_RESIZABLE);
    if (!s_game_window) return NULL;

    s_game_window->on_paint = game_paint;
    s_game_window->on_key = game_on_key;

    /* Register game with Gaming Mode subsystem */
    pid_t pid = process_get_current() ? process_get_current()->pid : 1;
    gaming_register_game(pid, "ZERO Strike");

    KLOG_INFO("game", "launched native demo game 'ZERO Strike' (PID: %d)", pid);
    return s_game_window;
}

bool demo_game_is_open(void)
{
    return (s_game_window != NULL);
}
