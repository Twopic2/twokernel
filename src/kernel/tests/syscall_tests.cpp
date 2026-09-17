#include <tests/ktest.hpp>
#include <sys/syscalls.hpp>
#include <arch/x86-64/arch/arch_syscalls.hpp>
#include <arch/x86-64/proc/process.hpp>
#include <arch/x86-64/proc/scheduler.hpp>
#include <arch/x86-64/proc/thread.hpp>

#include <cstddef>
#include <cstdint>

/*
    Syscall dispatch tests.

    These run entirely in ring 0: a SyscallFrame is built on the kernel stack
    and handed straight to do_syscall_64, the same pointer syscall.s would
    pass. That covers the table, the argument mapping and the return path
    without needing a user address space -- but by the same token it cannot
    see the SYSCALL/SYSRET transition. The ring 3 half lives in src/user.

    Requires init_syscalls() to have run (Cpu::init_cpu() does it).
*/

namespace Tests {
    namespace {
        using x86::Syscalls::SyscallFrame;
        namespace Sched = x86::Proc::Scheduler;

        /* A free slot below MAX_SYSCALLS that the real table never fills. */
        constexpr std::uint64_t PROBE_SLOT = 400;
        constexpr std::uint64_t PROBE_RETURN = 0xC0FFEEULL;

        SyscallFrame probe_seen {};
        bool probe_called = false;

        std::uint64_t probe_handler(SyscallFrame* frame) {
            probe_seen = *frame;
            probe_called = true;
            return PROBE_RETURN;
        }

        /* Installs probe_handler at PROBE_SLOT and puts the slot back after,
           so a failing case cannot leave the live table modified. */
        struct ProbeGuard {
            Syscalls::syscall_fn prev;

            ProbeGuard() : prev(Syscalls::global_syscall[PROBE_SLOT]) {
                Syscalls::global_syscall[PROBE_SLOT] = probe_handler;
                probe_seen = SyscallFrame {};
                probe_called = false;
            }

            ~ProbeGuard() {
                Syscalls::global_syscall[PROBE_SLOT] = prev;
            }
        };

        /* Handlers reach the current process through the scheduler, so a test
           has to stand one up. Swapping curr_thread is enough -- no queue
           surgery -- and it is restored on scope exit. */
        struct CurrentThreadGuard {
            x86::Proc::Thread::ThreadBlock* prev;

            explicit CurrentThreadGuard(x86::Proc::Thread::ThreadBlock* thread)
                : prev(Sched::g_scheduler.curr_thread) {
                Sched::g_scheduler.curr_thread = thread;
            }

            ~CurrentThreadGuard() {
                Sched::g_scheduler.curr_thread = prev;
            }
        };
    }

    /*
        The asm pushes rax last, so rax must sit at offset 0 and the rest must
        follow in the reverse of the push order. Reordering the struct silently
        breaks every argument accessor, which is why this is spelled out.
    */
    TEST_CASE("SyscallFrame layout matches the push order in syscall.s", "[syscall]") {
        CHECK(offsetof(SyscallFrame, rax) == 0);
        CHECK(offsetof(SyscallFrame, rbx) == 8);
        CHECK(offsetof(SyscallFrame, rcx) == 16);
        CHECK(offsetof(SyscallFrame, rdx) == 24);
        CHECK(offsetof(SyscallFrame, rdi) == 32);
        CHECK(offsetof(SyscallFrame, rsi) == 40);
        CHECK(offsetof(SyscallFrame, rbp) == 48);
        CHECK(offsetof(SyscallFrame, r8) == 56);
        CHECK(offsetof(SyscallFrame, r9) == 64);
        CHECK(offsetof(SyscallFrame, r10) == 72);
        CHECK(offsetof(SyscallFrame, r11) == 80);
        CHECK(offsetof(SyscallFrame, r12) == 88);
        CHECK(offsetof(SyscallFrame, r13) == 96);
        CHECK(offsetof(SyscallFrame, r14) == 104);
        CHECK(offsetof(SyscallFrame, r15) == 112);
        CHECK(sizeof(SyscallFrame) == 15 * 8);
    }

    /*
        The syscall instruction destroys rcx, so the fourth argument travels in
        r10 rather than the rcx the C ABI would use. Getting that one wrong is
        invisible until a 4-argument syscall appears.
    */
    TEST_CASE("argument accessors map to the syscall register order", "[syscall]") {
        SyscallFrame frame {};
        frame.rax = 39;
        frame.rdi = 0xA0;
        frame.rsi = 0xA1;
        frame.rdx = 0xA2;
        frame.r10 = 0xA3;
        frame.r8  = 0xA4;
        frame.r9  = 0xA5;
        frame.rcx = 0xDEAD;

        CHECK(frame.num() == 39);
        CHECK(frame.arg0() == 0xA0);
        CHECK(frame.arg1() == 0xA1);
        CHECK(frame.arg2() == 0xA2);
        CHECK(frame.arg3() == 0xA3);
        CHECK(frame.arg4() == 0xA4);
        CHECK(frame.arg5() == 0xA5);
        CHECK(frame.ret() == &frame.rax);
    }

    TEST_CASE("do_syscall_64 dispatches on rax and returns through rax", "[syscall]") {
        ProbeGuard guard {};

        SyscallFrame frame {};
        frame.rax = PROBE_SLOT;

        Syscalls::do_syscall_64(&frame);

        CHECK(probe_called);
        CHECK(frame.rax == PROBE_RETURN);
    }

    TEST_CASE("do_syscall_64 forwards all six arguments unmodified", "[syscall]") {
        ProbeGuard guard {};

        SyscallFrame frame {};
        frame.rax = PROBE_SLOT;
        frame.rdi = 0x11;
        frame.rsi = 0x22;
        frame.rdx = 0x33;
        frame.r10 = 0x44;
        frame.r8  = 0x55;
        frame.r9  = 0x66;

        Syscalls::do_syscall_64(&frame);

        REQUIRE(probe_called);
        CHECK(probe_seen.arg0() == 0x11);
        CHECK(probe_seen.arg1() == 0x22);
        CHECK(probe_seen.arg2() == 0x33);
        CHECK(probe_seen.arg3() == 0x44);
        CHECK(probe_seen.arg4() == 0x55);
        CHECK(probe_seen.arg5() == 0x66);
    }

    TEST_CASE("do_syscall_64 ignores numbers past the end of the table", "[syscall]") {
        constexpr std::uint64_t sentinel = 0x5A5A5A5A5A5A5A5AULL;

        SECTION("exactly one past the end") {
            SyscallFrame frame {};
            frame.rax = Syscalls::MAX_SYSCALLS;
            frame.rbx = sentinel;

            Syscalls::do_syscall_64(&frame);

            /* The early return leaves rax alone rather than writing a result. */
            CHECK(frame.rax == Syscalls::MAX_SYSCALLS);
            CHECK(frame.rbx == sentinel);
        }

        SECTION("a wild number from userspace") {
            SyscallFrame frame {};
            frame.rax = ~0ULL;

            Syscalls::do_syscall_64(&frame);

            CHECK(frame.rax == ~0ULL);
        }
    }

    TEST_CASE("init_syscalls filled the documented table slots", "[syscall]") {
        CHECK(Syscalls::global_syscall[1] != nullptr);   /* write  */
        CHECK(Syscalls::global_syscall[9] != nullptr);   /* mmap   */
        CHECK(Syscalls::global_syscall[12] != nullptr);  /* brk    */
        CHECK(Syscalls::global_syscall[39] != nullptr);  /* getpid */
        CHECK(Syscalls::global_syscall[58] != nullptr);  /* fork   */
        CHECK(Syscalls::global_syscall[60] != nullptr);  /* exit   */
    }

    /*
        Unregistered slots are null, and do_syscall_64 only range-checks the
        number -- so any of these from ring 3 is an indirect call through null.
        This case documents the hole; once the dispatcher returns -ENOSYS the
        assertion below should move to checking the returned value instead.
    */
    TEST_CASE("unregistered table slots are still null", "[syscall]") {
        CHECK(Syscalls::global_syscall[3] == nullptr);
        CHECK(Syscalls::global_syscall[Syscalls::MAX_SYSCALLS - 1] == nullptr);
    }

    TEST_CASE("getpid returns the pid of the current process", "[syscall]") {
        CurrentThreadGuard guard {&Sched::global_idle};

        auto* proc = Sched::g_scheduler.get_current_process();
        REQUIRE(proc != nullptr);

        SyscallFrame frame {};
        frame.rax = 39;

        Syscalls::do_syscall_64(&frame);

        CHECK(frame.rax == proc->pid);

        SECTION("and is stable across calls") {
            SyscallFrame again {};
            again.rax = 39;
            Syscalls::do_syscall_64(&again);
            CHECK(again.rax == frame.rax);
        }
    }

    /*
        exit() is destructive, so it runs against a throwaway process rather
        than the live one. The ProcessBlock keeps its pagemap page -- there is
        no process teardown yet, so the test leaks it deliberately.
    */
    TEST_CASE("exit parks the current process as a zombie", "[syscall]") {
        x86::Proc::Process::ProcessBlock proc {"exit_test", true};
        x86::Proc::Thread::ThreadBlock thread {};
        proc.add_thread(&thread);

        REQUIRE(proc.status == x86::Proc::Process::ProcStats::Ready);
        REQUIRE(proc.thread_size == 1);

        {
            CurrentThreadGuard guard {&thread};

            SyscallFrame frame {};
            frame.rax = 60;
            frame.rdi = 42;

            Syscalls::do_syscall_64(&frame);

            /* The status is echoed back, which is the only channel a user
               program has for reporting results right now. */
            CHECK(frame.rax == 42);
        }

        CHECK(proc.status == x86::Proc::Process::ProcStats::Zombie);

        /* EXPECTED TO FAIL today: remove_all_threads() calls clear_all(),
           which only nulls the deque's head and tail -- thread_size is never
           decremented, so the count stays stale after every thread is gone. */
        CHECK(proc.thread_size == 0);

        thread.exit();
    }
}
