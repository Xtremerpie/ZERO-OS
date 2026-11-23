; ZERO KERNEL - x86_64 Context Switch Routine (Version 2.0.0 System Architecture).
;
; Prototype:
;   void context_switch(struct cpu_context *prev, struct cpu_context *next);
;
; SysV ABI registers:
;   RDI = prev context pointer
;   RSI = next context pointer

bits 64
global context_switch

section .text

context_switch:
    ; 1. Save callee-saved registers of current thread into prev
    mov [rdi + 0*8], r15
    mov [rdi + 1*8], r14
    mov [rdi + 2*8], r13
    mov [rdi + 3*8], r12
    mov [rdi + 4*8], rbx
    mov [rdi + 5*8], rbp

    ; Save return RIP from top of stack
    mov rax, [rsp]
    mov [rdi + 6*8], rax

    ; Save RFLAGS
    pushfq
    pop qword [rdi + 7*8]

    ; Save RSP
    mov [rdi + 8*8], rsp

    ; 2. Switch stack and restore registers from next
    mov rsp, [rsi + 8*8]

    ; Restore RFLAGS
    push qword [rsi + 7*8]
    popfq

    ; Restore callee-saved registers
    mov rbp, [rsi + 5*8]
    mov rbx, [rsi + 4*8]
    mov r12, [rsi + 3*8]
    mov r13, [rsi + 2*8]
    mov r14, [rsi + 1*8]
    mov r15, [rsi + 0*8]
    ; Initial argument registers for a newly-created thread.
    mov rdi, [rsi + 9*8]
    mov rsi, [rsi + 10*8]

    ; 3. Jump to next RIP via return address placement
    mov rax, [rsi + 6*8]
    mov [rsp], rax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
