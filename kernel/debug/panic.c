/* ZERO KERNEL - panic handler. Prints diagnostics to serial + framebuffer, then halts. */

#include <stdbool.h>
#include <stdint.h>
#include "zero/panic.h"
#include "zero/log.h"
#include "zero/fbcon.h"
#include "zero/serial.h"
#include "zero/cpu.h"
#include "zero/selftest.h"
#include "zero/version.h"

static volatile bool panicking;

static void panic_begin(void)
{
    /* A fault inside the panic path (e.g. printing a bad pointer) must not recurse.
     * Report on serial ONLY - the framebuffer/printf code may be the thing that faulted. */
    if (panicking) {
        static const char msg[] = "\n*** NESTED PANIC: fault while panicking - halting ***\n";
        for (const char *p = msg; *p; p++)
            serial_putc(*p);
        test_device_exit(false);
        cpu_halt_forever();
    }
    panicking = true;
    __asm__ volatile("cli");
    fbcon_set_fg(0xD0283A);
    kprintf("\n*** ZERO KERNEL PANIC *** (ZERO OS %s, build %s)\n",
            ZERO_VERSION_STRING, ZERO_BUILD_ID);
}

__attribute__((noreturn)) static void panic_end(void)
{
    kprintf("System halted.\n");
    fbcon_set_fg(0xE8E8E8);
    test_device_exit(false); /* returns unless zero.test_exit=1 */
    cpu_halt_forever();
}

void kpanic(const char *fmt, ...)
{
    panic_begin();

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n");

    panic_end();
}

void kpanic_exception(const struct interrupt_frame *f)
{
    panic_begin();

    kprintf("Unhandled CPU exception: vector 0x%02lx %s\n", f->vector, exception_name(f->vector));
    kprintf("error code: 0x%016lx\n", f->error_code);
    uint64_t handler_sp;
    __asm__ volatile("movq %%rsp, %0" : "=r"(handler_sp));
    kprintf("handler stack: 0x%016lx\n", handler_sp); /* #DF must be inside the IST1 stack */
    if (f->vector == 14)
        kprintf("CR2 (fault address): 0x%016lx\n", read_cr2());

    kprintf("RIP=%016lx CS=%04lx RFLAGS=%016lx\n", f->rip, f->cs, f->rflags);
    kprintf("RSP=%016lx SS=%04lx CR3=%016lx\n", f->rsp, f->ss, read_cr3());
    kprintf("RAX=%016lx RBX=%016lx RCX=%016lx\n", f->rax, f->rbx, f->rcx);
    kprintf("RDX=%016lx RSI=%016lx RDI=%016lx\n", f->rdx, f->rsi, f->rdi);
    kprintf("RBP=%016lx R8 =%016lx R9 =%016lx\n", f->rbp, f->r8, f->r9);
    kprintf("R10=%016lx R11=%016lx R12=%016lx\n", f->r10, f->r11, f->r12);
    kprintf("R13=%016lx R14=%016lx R15=%016lx\n", f->r13, f->r14, f->r15);

    panic_end();
}
