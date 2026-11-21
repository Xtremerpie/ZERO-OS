/* ZERO OS SDK Sample Application - ZERO Intelligence Hello World (Milestone 8.0.0)
 *
 * Demonstrates:
 * 1. Initializing an AI conversational session via SDK API.
 * 2. Submitting user prompt to the local AI daemon.
 * 3. Rendering AI stream response in a native ZERO window.
 * 4. Interacting with the permission-gated automation broker.
 */
#include "zero/app.h"
#include "zero/ai.h"

static zero_app_config_t s_app_config = {
    .app_id = "zero.ai_hello",
    .name = "ZERO AI Demo",
    .version = "1.0.0",
    .window_width = 520,
    .window_height = 340
};

static zero_ai_session_t s_session = ZERO_AI_INVALID_SESSION;
static char s_response_buffer[512] = "Initializing ZERO Intelligence session...";
static bool s_prompt_sent = false;

static void on_paint(zero_window_t *win, zero_surface_t *surf)
{
    (void)win;

    /* Background */
    zero_draw_fill_rect(surf, 0, 0, surf->width, surf->height, ZERO_COLOR_PANEL);

    /* Decorative header banner */
    zero_draw_fill_rect(surf, 0, 0, surf->width, 36, ZERO_COLOR_PANEL_ALT);

    /* Gold accent bar */
    zero_draw_fill_rect(surf, 16, 50, surf->width - 32, 2, ZERO_COLOR_GOLD);

    /* AI Response Card */
    zero_draw_fill_rect(surf, 24, 70, surf->width - 48, 180, 0x14141Cu);
    zero_draw_rect(surf, 24, 70, surf->width - 48, 180, ZERO_COLOR_BORDER);

    /* Interactive button */
    zero_draw_fill_rect(surf, 24, 270, 140, 32, ZERO_COLOR_GOLD);
}

static void on_event(zero_window_t *win, const zero_event_t *evt)
{
    if (evt->type == ZERO_EVENT_MOUSE_DOWN) {
        if (!s_prompt_sent && s_session != ZERO_AI_INVALID_SESSION) {
            size_t out_len = 0;
            const char *prompt = "Explain ZERO OS architecture in one sentence.";
            zero_ai_err_t res = zero_ai_prompt(s_session, prompt, s_response_buffer,
                                               sizeof(s_response_buffer), &out_len);
            if (res == ZERO_AI_SUCCESS) {
                s_prompt_sent = true;
            }
            zero_window_invalidate(win);
        }
    } else if (evt->type == ZERO_EVENT_CLOSE) {
        if (s_session != ZERO_AI_INVALID_SESSION) {
            zero_ai_session_close(s_session);
            s_session = ZERO_AI_INVALID_SESSION;
        }
        win->running = false;
    }
}

/* Application entry point */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Initialize AI session */
    if (zero_ai_session_open("DemoSession", &s_session) == ZERO_AI_SUCCESS) {
        /* Request a friendly system notification through automation broker */
        zero_ai_action_t act;
        zero_ai_request_action(ZERO_AI_ACTION_NOTIFICATION, "AI Demo",
                               "ZERO Intelligence SDK initialized successfully", &act);
    }

    return zero_app_run(&s_app_config, on_paint, on_event);
}
