/* ZERO KERNEL - ZERO Terminal Subsystem Implementation (Milestone 5.0.0 ZERO Desktop).
 */
#include "desktop/terminal.h"
#include "desktop/compositor.h"
#include "desktop/login.h"
#include "desktop/shell.h"
#include "drivers/audio.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "apps/zpm.h"
#include "graphics/draw.h"
#include "graphics/font.h"
#include "graphics/color.h"
#include "graphics/gpu.h"
#include "hardware/acpi.h"
#include "hardware/pci.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "zero/string.h"
#include "zero/timer.h"
#include "zero/version.h"
#include <stdio.h>

static window_t *s_term_win = NULL;
static char      s_lines[TERM_MAX_LINES][TERM_MAX_COLS];
static uint32_t  s_colors[TERM_MAX_LINES];
static uint32_t  s_line_count = 0;
static char      s_input[128] = "";
static size_t    s_input_len = 0;
static char      s_cwd[VFS_MAX_PATH] = "/";

void terminal_puts_color(const char *str, uint32_t color)
{
    if (!str) return;

    if (s_line_count >= TERM_MAX_LINES) {
        /* Scroll lines up */
        for (uint32_t i = 0; i < TERM_MAX_LINES - 1; i++) {
            memcpy(s_lines[i], s_lines[i + 1], TERM_MAX_COLS);
            s_colors[i] = s_colors[i + 1];
        }
        s_line_count = TERM_MAX_LINES - 1;
    }

    size_t len = strlen(str);
    if (len >= TERM_MAX_COLS) len = TERM_MAX_COLS - 1;
    memcpy(s_lines[s_line_count], str, len);
    s_lines[s_line_count][len] = '\0';
    s_colors[s_line_count] = color;
    s_line_count++;
    compositor_mark_dirty();
}

void terminal_puts(const char *str)
{
    terminal_puts_color(str, COLOR_ZERO_WHITE);
}

void terminal_clear(void)
{
    s_line_count = 0;
    for (uint32_t i = 0; i < TERM_MAX_LINES; i++) {
        s_lines[i][0] = '\0';
    }
    compositor_mark_dirty();
}

void terminal_execute_command(const char *cmd_line)
{
    if (!cmd_line || cmd_line[0] == '\0') {
        return;
    }

    /* Print command echo */
    char echo[TERM_MAX_COLS];
    strcpy(echo, "zero@os$ ");
    strncat(echo, cmd_line, TERM_MAX_COLS - 11);
    terminal_puts_color(echo, COLOR_ZERO_GOLD);

    char buf[TERM_MAX_COLS];

    /* Command parsing */
    if (strcmp(cmd_line, "help") == 0) {
        terminal_puts_color("ZERO OS Shell - Commands:", COLOR_ZERO_GOLD);
        terminal_puts("  help, clear, version, uptime, memory, cpu, devices");
        terminal_puts("  storage, network, processes, pwd, cd, ls, cat <file>");
        terminal_puts("  zpm <cmd>, lock, shutdown");
    } else if (strcmp(cmd_line, "clear") == 0) {
        terminal_clear();
    } else if (strcmp(cmd_line, "version") == 0) {
        terminal_puts_color("ZERO OS " ZERO_VERSION_STRING " \"" ZERO_CODENAME "\"", COLOR_ZERO_GOLD);
        terminal_puts("Build: " ZERO_BUILD_ID " (x86_64 Higher-Half Ring 0 Kernel)");
    } else if (strncmp(cmd_line, "zpm", 3) == 0) {
        char out_buf[1024];
        zpm_exec_cli(cmd_line, out_buf, sizeof(out_buf));
        char *line = out_buf;
        while (*line) {
            char *line_end = line;
            while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
            char saved = *line_end;
            *line_end = '\0';
            if (*line) {
                terminal_puts(line);
            }
            if (saved == '\0') break;
            line = line_end + 1;
        }
    } else if (strcmp(cmd_line, "memory") == 0) {
        struct pmm_stats ps;
        struct heap_stats hs;
        pmm_get_stats(&ps);
        heap_get_stats(&hs);
        terminal_puts_color("PMM / VMM / Heap Memory (Live):", COLOR_ZERO_GOLD);
        snprintf(buf, sizeof(buf), "  Usable: %lu MiB | Free: %lu MiB (%lu frames)",
                 (unsigned long)(ps.usable_memory / (1024u * 1024u)),
                 (unsigned long)(ps.free_memory / (1024u * 1024u)),
                 (unsigned long)ps.free_frames);
        terminal_puts(buf);
        snprintf(buf, sizeof(buf), "  Heap Active: %lu KiB | Peak: %lu KiB",
                 (unsigned long)(hs.active_allocated_bytes / 1024u),
                 (unsigned long)(hs.peak_allocated_bytes / 1024u));
        terminal_puts(buf);
    } else if (strcmp(cmd_line, "cpu") == 0) {
        const struct acpi_madt_info *madt = acpi_get_madt_info();
        uint32_t cpus = (madt && madt->cpu_count > 0) ? madt->cpu_count : 1;
        terminal_puts_color("Processor & Timer (Live):", COLOR_ZERO_GOLD);
        snprintf(buf, sizeof(buf), "  Arch: x86_64 Long Mode | MADT CPUs: %u", cpus);
        terminal_puts(buf);
        snprintf(buf, sizeof(buf), "  APIC Timer: %u Hz | Ticks: %lu",
                 TIMER_DEFAULT_HZ, (unsigned long)timer_get_ticks());
        terminal_puts(buf);
    } else if (strcmp(cmd_line, "devices") == 0) {
        size_t n = pci_device_count();
        const gpu_device_t *gpu = gpu_get_device();
        const audio_device_t *aud = audio_get_device();
        snprintf(buf, sizeof(buf), "PCI Devices (%u):", (unsigned)n);
        terminal_puts_color(buf, COLOR_ZERO_GOLD);
        for (size_t i = 0; i < n && i < 8; i++) {
            const pci_device_t *d = pci_get_device(i);
            if (!d) continue;
            snprintf(buf, sizeof(buf), "  [%02x:%02x.%x] %04x:%04x %s",
                     d->bus, d->slot, d->func, d->vendor_id, d->device_id,
                     pci_class_name(d->class_code, d->subclass));
            terminal_puts(buf);
        }
        snprintf(buf, sizeof(buf), "  Display: %s (Accel: %s)",
                 gpu ? gpu->name : "Unknown",
                 (gpu && gpu->is_accelerated) ? "YES" : "NO (SW)");
        terminal_puts(buf);
        snprintf(buf, sizeof(buf), "  Audio: %s (Software Null Sink)",
                 aud ? aud->name : "None");
        terminal_puts(buf);
    } else if (strcmp(cmd_line, "storage") == 0) {
        terminal_puts_color("Storage & Filesystem Subsystem:", COLOR_ZERO_GOLD);
        terminal_puts("  /     - RAMFS In-Memory Root Filesystem");
        terminal_puts("  /data - ZEROFS v1 Volume on ramdisk0 (1 MiB, in-memory)");
    } else if (strcmp(cmd_line, "network") == 0) {
        terminal_puts_color("Network Interface (Local Stack / No Internet):", COLOR_ZERO_GOLD);
        terminal_puts("  Interface: eth0 (e1000) | Static Config: 10.0.2.15/24");
        terminal_puts("  Note: TCP/IP stack initialized; external internet unverified.");
    } else if (strcmp(cmd_line, "processes") == 0) {
        struct scheduler_stats st;
        scheduler_get_stats(&st);
        terminal_puts_color("Active Kernel Processes (Live):", COLOR_ZERO_GOLD);
        for (pid_t pid = 0; pid < (pid_t)MAX_PROCESSES; pid++) {
            process_t *p = process_get(pid);
            if (p && p->state != PROCESS_STATE_UNUSED) {
                snprintf(buf, sizeof(buf), "  PID %d  %-16s  threads=%u",
                         p->pid, p->name, p->thread_count);
                terminal_puts(buf);
            }
        }
        snprintf(buf, sizeof(buf), "  Switches: %lu | Idle ticks: %lu",
                 (unsigned long)st.context_switches, (unsigned long)st.idle_ticks);
        terminal_puts(buf);
    } else if (strcmp(cmd_line, "uptime") == 0) {
        uint64_t ms = timer_get_uptime_ms();
        snprintf(buf, sizeof(buf), "Uptime: %lu.%lu s (%lu ticks @ %u Hz)",
                 (unsigned long)(ms / 1000ull), (unsigned long)((ms % 1000ull) / 100ull),
                 (unsigned long)timer_get_ticks(), TIMER_DEFAULT_HZ);
        terminal_puts(buf);
    } else if (strcmp(cmd_line, "pwd") == 0) {
        terminal_puts(s_cwd);
    } else if (strncmp(cmd_line, "cd", 2) == 0 && (cmd_line[2] == '\0' || cmd_line[2] == ' ')) {
        const char *arg = cmd_line + 2;
        while (*arg == ' ') arg++;
        if (*arg == '\0' || strcmp(arg, "/") == 0) {
            strcpy(s_cwd, "/");
        } else {
            char next[VFS_MAX_PATH];
            if (arg[0] == '/') {
                strncpy(next, arg, sizeof(next) - 1);
                next[sizeof(next) - 1] = '\0';
            } else if (strcmp(s_cwd, "/") == 0) {
                snprintf(next, sizeof(next), "/%s", arg);
            } else {
                snprintf(next, sizeof(next), "%s/%s", s_cwd, arg);
            }
            file_t *d = vfs_open(next, O_RDONLY);
            if (d && d->vnode && d->vnode->type == VNODE_TYPE_DIR) {
                strcpy(s_cwd, next);
                vfs_close(d);
            } else {
                if (d) vfs_close(d);
                terminal_puts_color("cd: directory not found in VFS", COLOR_ZERO_CRIMSON);
            }
        }
    } else if (strcmp(cmd_line, "ls") == 0) {
        snprintf(buf, sizeof(buf), "Directory %s (VFS):", s_cwd);
        terminal_puts_color(buf, COLOR_ZERO_GOLD);
        file_t *d = vfs_open(s_cwd, O_RDONLY);
        if (d && d->vnode && d->vnode->type == VNODE_TYPE_DIR && d->vnode->fs_data) {
            ramfs_node_t *node = (ramfs_node_t *)d->vnode->fs_data;
            if (node->child_count == 0) {
                terminal_puts("  (empty)");
            } else {
                for (size_t i = 0; i < node->child_count; i++) {
                    ramfs_node_t *c = node->children[i];
                    if (!c) continue;
                    snprintf(buf, sizeof(buf), "  %-4s  %s (%lu B)",
                             (c->type == VNODE_TYPE_DIR) ? "DIR" : "FILE",
                             c->name, (unsigned long)c->size);
                    terminal_puts(buf);
                }
            }
            vfs_close(d);
        } else {
            if (d) vfs_close(d);
            terminal_puts("  (cannot open directory)");
        }
    } else if (strncmp(cmd_line, "cat", 3) == 0) {
        const char *arg = cmd_line + 3;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            terminal_puts("Usage: cat <filename>");
        } else {
            char path[VFS_MAX_PATH];
            if (arg[0] == '/') {
                strncpy(path, arg, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            } else if (strcmp(s_cwd, "/") == 0) {
                snprintf(path, sizeof(path), "/%s", arg);
            } else {
                snprintf(path, sizeof(path), "%s/%s", s_cwd, arg);
            }
            file_t *f = vfs_open(path, O_RDONLY);
            if (f) {
                char read_buf[TERM_MAX_COLS];
                int64_t bytes = vfs_read(f, read_buf, sizeof(read_buf) - 1);
                if (bytes > 0) {
                    read_buf[bytes] = '\0';
                    terminal_puts(read_buf);
                } else {
                    terminal_puts("(empty file)");
                }
                vfs_close(f);
            } else {
                terminal_puts("cat: file not found");
            }
        }
    } else if (strcmp(cmd_line, "lock") == 0) {
        login_lock();
        shell_set_state(SHELL_STATE_LOGIN);
    } else if (strcmp(cmd_line, "shutdown") == 0 || strcmp(cmd_line, "poweroff") == 0) {
        terminal_puts_color("Shutting down ZERO OS...", COLOR_ZERO_GOLD);
        shell_shutdown();
    } else {
        char err[TERM_MAX_COLS];
        strcpy(err, "Command not recognized: ");
        strncat(err, cmd_line, TERM_MAX_COLS - 26);
        terminal_puts_color(err, COLOR_ZERO_CRIMSON);
        terminal_puts("Type 'help' to see valid commands.");
    }
}

static void term_paint(window_t *win, framebuffer_t *fb)
{
    int32_t ox = win->x + 1;
    int32_t oy = win->y + WINDOW_TITLEBAR_HEIGHT;
    int32_t cw = win->client_width;
    int32_t ch = win->client_height;

    /* Terminal background: Deep Void Black */
    draw_fill_rect(fb, ox, oy, cw, ch, COLOR_ZERO_BLACK);

    /* Scroll to show most recent lines while keeping prompt visible */
    uint32_t max_rows = (ch > 28) ? (uint32_t)((ch - 28) / 12) : 1u;
    uint32_t start = (s_line_count > max_rows) ? (s_line_count - max_rows) : 0u;

    int32_t line_y = oy + 6;
    for (uint32_t i = start; i < s_line_count && line_y < oy + ch - 22; i++) {
        font_draw_string(fb, ox + 8, line_y, s_lines[i], 1, s_colors[i]);
        line_y += 12;
    }

    /* Prompt line */
    const char *prompt = "zero@os$ ";
    int32_t pw = font_get_string_width(prompt, 1);
    font_draw_string(fb, ox + 8, line_y, prompt, 1, COLOR_ZERO_GOLD);

    /* Input text */
    font_draw_string(fb, ox + 8 + pw, line_y, s_input, 1, COLOR_ZERO_WHITE);

    /* Terminal block cursor */
    int32_t in_w = font_get_string_width(s_input, 1);
    draw_fill_rect(fb, ox + 8 + pw + in_w, line_y + 1, 7, 9, COLOR_ZERO_GOLD);
}

static void term_key(window_t *win, uint8_t key, uint8_t scancode)
{
    (void)win;
    (void)scancode;

    if (key == '\n' || key == '\r' || key == 0x0A || key == 0x0D) {
        terminal_execute_command(s_input);
        s_input[0] = '\0';
        s_input_len = 0;
        compositor_mark_dirty();
        return;
    }

    if (key == '\b' || key == 0x08) {
        if (s_input_len > 0) {
            s_input_len--;
            s_input[s_input_len] = '\0';
            compositor_mark_dirty();
        }
        return;
    }

    if (key >= 32 && key <= 126) {
        if (s_input_len < sizeof(s_input) - 1) {
            s_input[s_input_len++] = (char)key;
            s_input[s_input_len] = '\0';
            compositor_mark_dirty();
        }
    }
}

static void term_close(window_t *win)
{
    (void)win;
    s_term_win = NULL;
}

window_t *terminal_launch(void)
{
    if (s_term_win) {
        wm_bring_to_front(s_term_win);
        wm_set_focus(s_term_win);
        compositor_mark_dirty();
        return s_term_win;
    }

    s_term_win = wm_create_window("ZERO Terminal", 40, 40, 580, 360, WINDOW_FLAG_RESIZABLE);
    if (s_term_win) {
        s_term_win->on_paint = term_paint;
        s_term_win->on_key = term_key;
        s_term_win->on_close = term_close;

        if (s_line_count == 0) {
            terminal_puts_color("ZERO OS " ZERO_VERSION_STRING " (" ZERO_CODENAME ")", COLOR_ZERO_GOLD);
            terminal_puts("Type 'help' for commands, or 'shutdown' to power off.");
        }
        compositor_mark_dirty();
    }

    return s_term_win;
}

void terminal_close(void)
{
    if (s_term_win) {
        wm_close(s_term_win);
        s_term_win = NULL;
    }
}

bool terminal_is_open(void)
{
    return (s_term_win != NULL);
}
