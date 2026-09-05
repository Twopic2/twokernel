.section .text
.global syscall_hanlder
.type syscall_hanlder, @function

syscall_hanlder:
    /* Switches to kernel_rsp from user_rsp*/
    swapgs
    movq %rsp, %gs:8    
    movq %gs:0, %rsp

    pushq %gs:8
    pushq %r11                      
    pushq %rcx                      

    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9
    push %r8
    push %rbp
    push %rsi
    push %rdi
    push %rdx
    push %rcx
    push %rbx
    push %rax

    cld

    mov %rsp, %rdi

    push %rbp
    mov %rsp, %rbp
    and $~15, %rsp

    call do_syscall_64 

    mov %rbp, %rsp
    pop %rbp

    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rdi
    pop %rsi
    pop %rbp
    pop %r8
    pop %r9
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    popq %rcx
    popq %r11
    swapgs
    popq %rsp

    sysretq

.section .note.GNU-stack
