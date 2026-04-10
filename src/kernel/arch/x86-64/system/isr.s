; Exceptions that push an error code automatically: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
; All others push a fake 0 so every handler sees the same stack layout.

; Macro for vectors WITHOUT an error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0        ; fake error code
    push qword %1       ; vector number
    jmp common_stub
%endmacro

; Macro for vectors WITH an error code (CPU already pushed it)
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1       ; vector number (error code already on stack)
    jmp common_stub
%endmacro

; CPU exceptions (0–31)
ISR_NOERR  0   ; Division by zero
ISR_NOERR  1   ; Debug
ISR_NOERR  2   ; Non-maskable interrupt
ISR_NOERR  3   ; Breakpoint
ISR_NOERR  4   ; Overflow
ISR_NOERR  5   ; Bound range exceeded
ISR_NOERR  6   ; Invalid opcode
ISR_NOERR  7   ; Device not available
ISR_ERR    8   ; Double fault
ISR_NOERR  9   ; Coprocessor segment overrun
ISR_ERR    10  ; Invalid TSS
ISR_ERR    11  ; Segment not present
ISR_ERR    12  ; Stack-segment fault
ISR_ERR    13  ; General protection fault
ISR_ERR    14  ; Page fault
ISR_NOERR  15  ; Reserved
ISR_NOERR  16  ; x87 floating-point
ISR_ERR    17  ; Alignment check
ISR_NOERR  18  ; Machine check
ISR_NOERR  19  ; SIMD floating-point
ISR_NOERR  20  ; Virtualization
ISR_ERR    21  ; Control protection
ISR_NOERR  22
ISR_NOERR  23
ISR_NOERR  24
ISR_NOERR  25
ISR_NOERR  26
ISR_NOERR  27
ISR_NOERR  28
ISR_NOERR  29
ISR_ERR    30  ; Security exception
ISR_NOERR  31

; Hardware IRQs (32–47) — no error codes
%assign i 32
%rep 16
    ISR_NOERR i
    %assign i i+1
%endrep

; ── Common stub ───────────────────────────────────────────────────────────────
; At this point the stack holds (top → bottom):
;   vector, error_code, rip, cs, rflags, rsp, ss  (CPU-pushed)
;
; We save all GP registers, call the C++ handler, then restore and iretq.

extern isr_dispatch     ; C++ handler: void isr_dispatch(InterruptFrame*)

common_stub:
    ; Save general-purpose registers (must match InterruptFrame layout in reverse)
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

    mov rdi, rsp        ; pass pointer to InterruptFrame as first argument
    call isr_dispatch

    ; Restore registers
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

    add rsp, 16         ; skip vector + error_code
    iretq
