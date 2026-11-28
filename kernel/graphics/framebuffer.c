/* ZERO KERNEL - Framebuffer Architecture & Double Buffer Implementation (Milestone 5.0.0 ZERO Desktop).
 *
 * Implements hardware framebuffer mapping, double buffering, damage region tracking,
 * and graphics diagnostic reporting.
 */
#include "graphics/framebuffer.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "lib/kprintf.h"
#include "lib/string.h"

static framebuffer_t g_primary_fb;
static spinlock_t    g_fb_lock;

bool fb_init(const struct limine_framebuffer *lfb)
{
    spinlock_init(&g_fb_lock);
    memset(&g_primary_fb, 0, sizeof(g_primary_fb));

    if (!lfb || lfb->bpp != 32 || lfb->memory_model != LIMINE_FRAMEBUFFER_RGB) {
        KLOG_WARN("graphics", "unsupported or missing framebuffer (bpp=%u, model=%u)",
                  lfb ? (uint32_t)lfb->bpp : 0, lfb ? (uint32_t)lfb->memory_model : 0);
        return false;
    }

    g_primary_fb.front_buffer = (uint32_t *)lfb->address;
    g_primary_fb.width = (uint32_t)lfb->width;
    g_primary_fb.height = (uint32_t)lfb->height;
    g_primary_fb.pitch = (uint32_t)lfb->pitch;
    g_primary_fb.pixels_per_pitch = g_primary_fb.pitch / 4u;
    g_primary_fb.bpp = (uint32_t)lfb->bpp;

    g_primary_fb.red_shift = lfb->red_mask_shift;
    g_primary_fb.green_shift = lfb->green_mask_shift;
    g_primary_fb.blue_shift = lfb->blue_mask_shift;

    /* Allocate system RAM double-buffer for tear-free rendering */
    size_t fb_bytes = (size_t)g_primary_fb.pitch * g_primary_fb.height;
    size_t pages_needed = (fb_bytes + VMM_PAGE_SIZE - 1) / VMM_PAGE_SIZE;

    /* Attempt allocation via PMM contiguous frames or heap (up to 16 MiB, e.g. 1920x1080x32) */
    void *back_mem = NULL;
    if (pages_needed <= 4096) {
        back_mem = kmalloc(fb_bytes);
    }

    if (back_mem) {
        g_primary_fb.back_buffer = (uint32_t *)back_mem;
        memset(g_primary_fb.back_buffer, 0, fb_bytes);
    } else {
        /* Fall back to direct front buffer rendering if memory is extremely tight */
        KLOG_WARN("graphics", "unable to allocate double-buffer (%u KiB); using direct front-buffer",
                  (uint32_t)(fb_bytes / 1024u));
        g_primary_fb.back_buffer = g_primary_fb.front_buffer;
    }

    fb_reset_clip(&g_primary_fb);
    g_primary_fb.is_damaged = false;
    g_primary_fb.initialized = true;

    KLOG_INFO("graphics", "framebuffer initialized: %ux%u @ %u bpp, pitch %u bytes, double-buffer: %s",
              g_primary_fb.width, g_primary_fb.height, g_primary_fb.bpp,
              g_primary_fb.pitch,
              (g_primary_fb.back_buffer != g_primary_fb.front_buffer) ? "ENABLED" : "DIRECT");

    return true;
}

framebuffer_t *fb_get_primary(void)
{
    return g_primary_fb.initialized ? &g_primary_fb : NULL;
}

bool fb_is_available(void)
{
    return g_primary_fb.initialized;
}

void fb_set_clip(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!fb) return;

    /* Clamp clip rect to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int32_t)fb->width)  w = (int32_t)fb->width - x;
    if (y + h > (int32_t)fb->height) h = (int32_t)fb->height - y;

    if (w < 0) w = 0;
    if (h < 0) h = 0;

    fb->clip.x = x;
    fb->clip.y = y;
    fb->clip.w = w;
    fb->clip.h = h;
}

void fb_reset_clip(framebuffer_t *fb)
{
    if (!fb) return;
    fb->clip.x = 0;
    fb->clip.y = 0;
    fb->clip.w = (int32_t)fb->width;
    fb->clip.h = (int32_t)fb->height;
}

void fb_mark_damage(framebuffer_t *fb, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!fb || w <= 0 || h <= 0) return;

    int32_t x1 = x + w;
    int32_t y1 = y + h;

    uint64_t flags;
    spin_lock_irqsave(&g_fb_lock, &flags);
    if (!fb->is_damaged) {
        fb->damage_x0 = x;
        fb->damage_y0 = y;
        fb->damage_x1 = x1;
        fb->damage_y1 = y1;
        fb->is_damaged = true;
    } else {
        if (x < fb->damage_x0) fb->damage_x0 = x;
        if (y < fb->damage_y0) fb->damage_y0 = y;
        if (x1 > fb->damage_x1) fb->damage_x1 = x1;
        if (y1 > fb->damage_y1) fb->damage_y1 = y1;
    }
    spin_unlock_irqrestore(&g_fb_lock, flags);
}

void fb_flip(framebuffer_t *fb)
{
    if (!fb || !fb->initialized || !fb->is_damaged) return;
    if (fb->front_buffer == fb->back_buffer) {
        fb->is_damaged = false;
        return;
    }

    uint64_t flags;
    spin_lock_irqsave(&g_fb_lock, &flags);

    /* Clamp damage rect to screen boundaries */
    int32_t x0 = (fb->damage_x0 < 0) ? 0 : fb->damage_x0;
    int32_t y0 = (fb->damage_y0 < 0) ? 0 : fb->damage_y0;
    int32_t x1 = (fb->damage_x1 > (int32_t)fb->width) ? (int32_t)fb->width : fb->damage_x1;
    int32_t y1 = (fb->damage_y1 > (int32_t)fb->height) ? (int32_t)fb->height : fb->damage_y1;

    if (x1 > x0 && y1 > y0) {
        size_t copy_bytes = (size_t)(x1 - x0) * sizeof(uint32_t);
        for (int32_t y = y0; y < y1; y++) {
            uint32_t offset = (uint32_t)y * fb->pixels_per_pitch + (uint32_t)x0;
            memcpy(&fb->front_buffer[offset], &fb->back_buffer[offset], copy_bytes);
        }
    }

    fb->is_damaged = false;
    spin_unlock_irqrestore(&g_fb_lock, flags);
}

void fb_flip_full(framebuffer_t *fb)
{
    if (!fb || !fb->initialized) return;
    fb_mark_damage(fb, 0, 0, (int32_t)fb->width, (int32_t)fb->height);
    fb_flip(fb);
}

void fb_clear(framebuffer_t *fb, uint32_t rgb)
{
    if (!fb || !fb->initialized) return;

    uint32_t packed = fb_pack_color(fb, rgb);
    for (uint32_t y = 0; y < fb->height; y++) {
        uint32_t row_start = y * fb->pixels_per_pitch;
        for (uint32_t x = 0; x < fb->width; x++) {
            fb->back_buffer[row_start + x] = packed;
        }
    }

    fb_mark_damage(fb, 0, 0, (int32_t)fb->width, (int32_t)fb->height);
}

void fb_dump_info(void)
{
    kprintf("\n================================================================================\n");
    kprintf("              ZERO OS GRAPHICS ARCHITECTURE & FRAMEBUFFER REPORT\n");
    kprintf("================================================================================\n");
    if (!g_primary_fb.initialized) {
        kprintf("  Status           : UNINITIALIZED (No active graphical framebuffer)\n");
    } else {
        kprintf("  Status           : ACTIVE & OPERATIONAL\n");
        kprintf("  Resolution       : %u x %u pixels\n", g_primary_fb.width, g_primary_fb.height);
        kprintf("  Color Depth      : %u bpp (32-bit TrueColor)\n", g_primary_fb.bpp);
        kprintf("  Scanline Pitch   : %u bytes (%u pixels/row)\n", g_primary_fb.pitch, g_primary_fb.pixels_per_pitch);
        kprintf("  Hardware VRAM    : 0x%p\n", (void *)g_primary_fb.front_buffer);
        kprintf("  Double Buffer    : 0x%p (%s)\n",
                (void *)g_primary_fb.back_buffer,
                (g_primary_fb.back_buffer != g_primary_fb.front_buffer) ? "RAM Double-Buffer Active" : "Direct VRAM");
        kprintf("  Color Shift (RGB): R+%u, G+%u, B+%u\n",
                g_primary_fb.red_shift, g_primary_fb.green_shift, g_primary_fb.blue_shift);
        kprintf("  Damage Tracking  : Active bounding box flush enabled\n");
    }
    kprintf("================================================================================\n\n");
}
