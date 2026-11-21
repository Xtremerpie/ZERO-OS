/* ZERO KERNEL - CPU identification and control-register report.
 *
 * CPUID strings are assembled from properly typed uint32_t words and copied with
 * memcpy: a char array is never accessed through a uint32_t pointer (that would
 * violate strict aliasing and alignment rules). */

#include <stdint.h>
#include "zero/cpu.h"
#include "zero/log.h"
#include "zero/string.h"

static void brand_string(char out[49])
{
    uint32_t max_ext, b, c, d;
    cpuid(0x80000000u, 0, &max_ext, &b, &c, &d);
    if (max_ext < 0x80000004u) {
        out[0] = '?';
        out[1] = '\0';
        return;
    }

    uint32_t words[12];
    for (uint32_t leaf = 0; leaf < 3; leaf++)
        cpuid(0x80000002u + leaf, 0, &words[leaf * 4 + 0], &words[leaf * 4 + 1],
              &words[leaf * 4 + 2], &words[leaf * 4 + 3]);
    memcpy(out, words, 48);
    out[48] = '\0';

    /* Brand strings are often left-padded with spaces. */
    char *p = out;
    while (*p == ' ')
        p++;
    if (p != out)
        memmove(out, p, strlen(p) + 1);
}

void cpu_report(void)
{
    uint32_t a, b, c, d;
    char vendor[13];

    cpuid(0, 0, &a, &b, &c, &d);
    uint32_t vwords[3] = { b, d, c }; /* vendor string order: EBX, EDX, ECX */
    memcpy(vendor, vwords, 12);
    vendor[12] = '\0';

    char brand[49];
    brand_string(brand);

    KLOG_INFO("cpu", "vendor=%s", vendor);
    KLOG_INFO("cpu", "brand=%s", brand);

    /* CPUID.1:EAX - Intel SDM vol. 2 "CPUID": the extended family is added only when the
     * base family is 0xF; the extended model is used only when the base family is 0x6 or 0xF. */
    cpuid(1, 0, &a, &b, &c, &d);
    uint32_t base_family = (a >> 8) & 0xF;
    uint32_t ext_family = (a >> 20) & 0xFF;
    uint32_t base_model = (a >> 4) & 0xF;
    uint32_t ext_model = (a >> 16) & 0xF;
    uint32_t family = base_family == 0xF ? base_family + ext_family : base_family;
    uint32_t model = (base_family == 0x6 || base_family == 0xF) ? (ext_model << 4) | base_model
                                                                : base_model;
    KLOG_INFO("cpu", "family/model/stepping: %u/%u/%u", family, model, a & 0xF);

    uint64_t efer = rdmsr(MSR_EFER);
    KLOG_INFO("cpu", "CR0=0x%lx CR3=0x%lx CR4=0x%lx", read_cr0(), read_cr3(), read_cr4());
    KLOG_INFO("cpu", "EFER=0x%lx (long mode %s, NX %s)", efer,
              (efer & EFER_LMA) ? "active" : "INACTIVE",
              (efer & EFER_NXE) ? "enabled" : "disabled");
}
