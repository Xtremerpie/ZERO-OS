/* ZERO KERNEL - Unified Input Subsystem Implementation (Milestone 3.0.0 Hardware Foundation). */

#include "drivers/input.h"
#include "zero/spinlock.h"
#include "zero/log.h"
#include "zero/string.h"

static input_event_t g_input_queue[INPUT_QUEUE_SIZE];
static size_t g_queue_head = 0;
static size_t g_queue_tail = 0;
static size_t g_queue_count = 0;
static spinlock_t g_input_lock;
static uint32_t g_active_modifiers = 0;

void input_init(void)
{
    spinlock_init(&g_input_lock);
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
    g_active_modifiers = 0;
    memset(g_input_queue, 0, sizeof(g_input_queue));
    KLOG_INFO("input", "Unified hardware input subsystem initialized (queue capacity: %u)", INPUT_QUEUE_SIZE);
}

int input_push_event(const input_event_t *ev)
{
    if (!ev)
        return -1;

    spinlock_acquire(&g_input_lock);

    if (g_queue_count >= INPUT_QUEUE_SIZE) {
        /* Drop oldest event to avoid lockups on queue overflow */
        g_queue_head = (g_queue_head + 1) % INPUT_QUEUE_SIZE;
        g_queue_count--;
    }

    g_input_queue[g_queue_tail] = *ev;
    g_queue_tail = (g_queue_tail + 1) % INPUT_QUEUE_SIZE;
    g_queue_count++;

    spinlock_release(&g_input_lock);
    return 0;
}

bool input_poll_event(input_event_t *out_ev)
{
    if (!out_ev)
        return false;

    spinlock_acquire(&g_input_lock);

    if (g_queue_count == 0) {
        spinlock_release(&g_input_lock);
        return false;
    }

    *out_ev = g_input_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1) % INPUT_QUEUE_SIZE;
    g_queue_count--;

    spinlock_release(&g_input_lock);
    return true;
}

uint32_t input_get_modifiers(void)
{
    return g_active_modifiers;
}

void input_set_modifier(uint32_t mod, bool active)
{
    if (active)
        g_active_modifiers |= mod;
    else
        g_active_modifiers &= ~mod;
}

char input_keycode_to_ascii(uint32_t keycode, uint32_t modifiers)
{
    bool shift = (modifiers & MODIFIER_SHIFT) != 0;
    bool caps = (modifiers & MODIFIER_CAPSLOCK) != 0;

    /* Letters A-Z */
    if (keycode >= KEY_A && keycode <= KEY_Z) {
        bool uppercase = (shift ^ caps);
        char base = uppercase ? 'A' : 'a';
        return (char)(base + (keycode - KEY_A));
    }

    /* Digits 0-9 */
    if (keycode >= KEY_0 && keycode <= KEY_9) {
        static const char shift_nums[] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
        if (shift) {
            return shift_nums[keycode - KEY_0];
        }
        return (char)('0' + (keycode - KEY_0));
    }

    switch (keycode) {
    case KEY_SPACE:        return ' ';
    case KEY_ENTER:        return '\n';
    case KEY_TAB:          return '\t';
    case KEY_BACKSPACE:    return '\b';
    case KEY_MINUS:        return shift ? '_' : '-';
    case KEY_EQUALS:       return shift ? '+' : '=';
    case KEY_LEFTBRACKET:  return shift ? '{' : '[';
    case KEY_RIGHTBRACKET: return shift ? '}' : ']';
    case KEY_BACKSLASH:    return shift ? '|' : '\\';
    case KEY_SEMICOLON:    return shift ? ':' : ';';
    case KEY_APOSTROPHE:   return shift ? '"' : '\'';
    case KEY_GRAVE:        return shift ? '~' : '`';
    case KEY_COMMA:        return shift ? '<' : ',';
    case KEY_PERIOD:       return shift ? '>' : '.';
    case KEY_SLASH:        return shift ? '?' : '/';
    default:               return '\0';
    }
}
