/*
    ! SYSCALL entry stub

    ? The CPU does almost nothing for us here: no stack switch, no register
    ? save. On entry rsp still points at the *user* stack, rcx holds the user
    ? rip and r11 the user rflags.

    ? Everything this pushes is arranged to reproduce the exact frame isr.s
    ? builds, so Syscalls::syscall_dispatch() can serve both this path and the
    ? int 0x80 path without knowing which door it came through.
*/

.section .text
.global syscall_hanlder
.type syscall_hanlder, @function

syscall_hanlder:
    /*
        ! First instruction, and it must not touch the stack.
        ? This store targets a kernel global via rip-relative addressing, so no
        ? push happens while rsp is still user-controlled.
    */
    movq %rsp, syscall_user_rsp(%rip)
    movq syscall_kernel_rsp(%rip), %rsp

    /*
        ? Hand-build the tail the CPU would have pushed for an interrupt.
        ? cs/ss track System::Idt::USER_CODE|3 and USER_DATA|3 -- update both
        ? if the GDT order changes for SYSRET.
    */
    pushq $0x23                     /* ss     -- USER_DATA | 3 */
    pushq syscall_user_rsp(%rip)    /* rsp                     */
    pushq %r11                      /* rflags                  */
    pushq $0x1b                     /* cs     -- USER_CODE | 3 */
    pushq %rcx                      /* rip                     */
    pushq $0                        /* error  -- isr.s pushes a dummy too */

    /*
        ? Same order as isr.s:19-63. Pushing rax first and r15 last is what
        ? leaves r15 at the lowest address, which is how IrqFrame declares it --
        ? that is the whole reason %rsp can be handed over as an IrqFrame*.
    */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rdi
    push %rsi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    cld

    mov %rsp, %rdi

    /* SysV wants rsp 16-byte aligned at the call; 21 pushes leaves it off by 8. */
    push %rbp
    mov %rsp, %rbp
    and $~15, %rsp

    call syscall_dispatch

    mov %rbp, %rsp
    pop %rbp

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rsi
    pop %rdi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    /*
        ? sysretq takes rip from rcx and rflags from r11, so those two come out
        ? of the frame rather than being popped into their own registers. cs and
        ? ss are rebuilt by the CPU from STAR, so both slots are skipped.
    */
    add $8, %rsp                    /* error  */
    popq %rcx                       /* rip    */
    add $8, %rsp                    /* cs     */
    popq %r11                       /* rflags */
    popq %rsp                       /* back onto the user stack */

    sysretq

.section .note.GNU-stack
