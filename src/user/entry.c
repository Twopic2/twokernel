#include "syscall.h"
#include "utest.h"

/*
    ? Userspace syscall tests, shaped like a Pintos user program: one main()
    ? per bootable image, results reported through the exit status.

    ? These cover what the kernel-side [syscall] tests cannot -- the ring 3
    ? transition itself: the SYSCALL/SYSRET pair, the GS-relative stack switch
    ? in syscall.s, and whether the stub gives back the registers it borrowed.
*/

/*
    ? Set to 1 once do_syscall_64 null-checks the table slot. Until then an
    ? unregistered number is an indirect call through a null pointer, so the
    ? check below would take the machine down instead of failing.
*/
#define UTEST_KERNEL_HAS_ENOSYS_GUARD 0

int main(void);

/*
    ! The ELF entry point MUST be named _start.
    ! CMakeLists links the user image with -Wl,-e,_start; when the symbol is
    ! missing lld only warns ("cannot find entry symbol") and writes
    ! e_entry = 0, so elf_load() hands the scheduler a null entry point.
*/
void _start(void) {
    exit(main());

    /* exit() must not return. If it ever does, spin here rather than run off
       the end of the mapped text. */
    for (;;) {}
}

/*
    ? syscall.s pushes and pops all 15 GPRs around do_syscall_64, so the
    ? callee-saved set must come back unchanged. rbx/r12-r15 are listed as
    ? clobbers (not operands) so the compiler saves them for us; the outputs
    ? are memory operands because rsp is untouched inside the block.
*/
static int syscall_preserves_callee_saved(void) {
    unsigned long rbx_out = 0;
    unsigned long r12_out = 0;
    unsigned long r13_out = 0;
    unsigned long r14_out = 0;
    unsigned long r15_out = 0;

    __asm__ volatile(
        "movq $0x1111111111111111, %%rbx\n\t"
        "movq $0x2222222222222222, %%r12\n\t"
        "movq $0x3333333333333333, %%r13\n\t"
        "movq $0x4444444444444444, %%r14\n\t"
        "movq $0x5555555555555555, %%r15\n\t"
        "movq $39, %%rax\n\t"
        "syscall\n\t"
        "movq %%rbx, %0\n\t"
        "movq %%r12, %1\n\t"
        "movq %%r13, %2\n\t"
        "movq %%r14, %3\n\t"
        "movq %%r15, %4\n\t"
        : "=m"(rbx_out), "=m"(r12_out), "=m"(r13_out),
          "=m"(r14_out), "=m"(r15_out)
        :
        : "rax", "rcx", "r11", "rbx", "r12", "r13", "r14", "r15", "memory");

    return rbx_out == 0x1111111111111111UL
        && r12_out == 0x2222222222222222UL
        && r13_out == 0x3333333333333333UL
        && r14_out == 0x4444444444444444UL
        && r15_out == 0x5555555555555555UL;
}

#if UTEST_KERNEL_HAS_ENOSYS_GUARD
/* Raw trap that skips the typed wrappers, so the number can be bogus. */
static long raw_syscall(long num) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}
#endif

int main(void) {
    UT_BEGIN();

    /* Read once up front: every later check reuses it. */
    long pid = getpid();

    /* 1. The trap completes and comes back with a plausible value in rax.
          A hang here means the entry path never returned; a wild value means
          the return never made it into the saved rax slot. */
    UT_CHECK(pid > 0);

    /* 2. getpid is stable. A second call must agree with the first --
          catches a stub that returns whatever happens to be in rax. */
    UT_CHECK(getpid() == pid);

    /* 3. The kernel stub restores the callee-saved registers. */
    UT_CHECK(syscall_preserves_callee_saved());

    /* 4. The user stack survives the round trip. This local is written
          before a syscall and read after: if the GS-relative switch in
          syscall.s returns the wrong rsp, the frame is gone. */
    volatile long canary = 0x5A5A5A5A;
    (void)getpid();
    UT_CHECK(canary == 0x5A5A5A5A);

#if UTEST_KERNEL_HAS_ENOSYS_GUARD
    /* 5. An unregistered number is rejected rather than dispatched. */
    UT_CHECK(raw_syscall(2) == -38);
#endif

    return UT_RESULT();
}
