.section .text
.global syscall_hanlder
.type syscall_hanlder, @function

syscall_hanlder:
    movq %rsp, syscall_user_rsp(%rip)
    movq syscall_kernel_rsp(%rip), %rsp

    pushq $0x23                     /* ss     -- USER_DATA | 3 */
    pushq syscall_user_rsp(%rip)    /* rsp                     */
    pushq %r11                      /* rflags                  */
    pushq $0x1b                     /* cs     -- USER_CODE | 3 */
    pushq %rcx                      /* rip                     */
    pushq $0                        /* error  -- isr.s pushes a dummy too */

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

    add $8, %rsp                    /* error  */
    popq %rcx                       /* rip    */
    add $8, %rsp                    /* cs     */
    popq %r11                       /* rflags */
    popq %rsp                       /* back onto the user stack */

    sysretq

.section .note.GNU-stack
