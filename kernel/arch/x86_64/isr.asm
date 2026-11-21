; ZERO KERNEL - exception entry stubs (vectors 0-31).
;
; Builds a `struct interrupt_frame` (kernel/include/zero/idt.h) on the stack and
; calls exception_dispatch(frame). Layout, lowest address first:
;   r15..r8, rbp, rdi, rsi, rdx, rcx, rbx, rax, vector, error_code,
;   rip, cs, rflags, rsp, ss
; The CPU always pushes ss/rsp/rflags/cs/rip (64-bit mode); some vectors also push
; an error code, for the rest we push 0 so the frame is uniform.

bits 64

extern exception_dispatch

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push qword %1
    jmp isr_common
%endmacro

section .text

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    cld
    mov rdi, rsp            ; arg0 = frame
    mov rbp, rsp            ; rbp is restored from the frame below
    and rsp, -16            ; SysV ABI: 16-byte aligned at the call
    call exception_dispatch
    mov rsp, rbp

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16             ; drop vector + error code
    iretq

; Vectors that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

; Vectors 32-47 for hardware interrupts (IRQ0-15 / APIC timer)
%assign i 32
%rep 16
ISR_NOERR i
%assign i i+1
%endrep

section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dq isr_stub_ %+ i
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
