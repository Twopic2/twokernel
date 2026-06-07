#include "memory/vmm.hpp"
#include <tests/ktest.hpp>
#include <arch/x86-64/proc/scheduler.hpp>
#include <arch/x86-64/proc/thread.hpp>
#include <arch/x86-64/system/gdt.hpp>
#include <cstdint>

/**  

*! Maybe I shouldn't vibe code the test cases 

*/

namespace Tests {
    using namespace x86::Proc;
    using namespace x86::Proc::Scheduler;

    void thread_a(void*) { for (;;) { Util::klog("A"); asm volatile("hlt"); } }
    void thread_b(void*) { for (;;) { Util::klog("B"); asm volatile("hlt"); } }

    void add_init_thread() {
        static Thread::ThreadBlock ta {};
        static Thread::ThreadBlock tb {};

        g_scheduler.add_ready_threads(&ta);
        g_scheduler.add_ready_threads(&tb);
    }

    void run_scheduler_tests() {
        Util::klog("--- Scheduler Tests ---\n");
        KTest::reset();

        auto reset_sched = [] {
            g_scheduler.ready_threads.clear_all();
            g_scheduler.curr_thread          = nullptr;
            g_scheduler.curr_ready_thread    = nullptr;
            g_scheduler.task_switch_counter  = 0;
            g_scheduler.disable_counter      = 0;
            g_scheduler.task_switch_postponed = false;
        };

        // ── A thread acquires its own kernel stack ───────────────────────────
        Thread::ThreadBlock t0 {};
        Thread::ThreadBlock t1 {};

        KTEST_ASSERT(t0.kernel_stack != t0.rsp0,
                     "thread stack base (kernel_stack) sits below its top (rsp0)");
        KTEST_ASSERT(reinterpret_cast<std::uintptr_t>(t0.rsp0) - reinterpret_cast<std::uintptr_t>(t0.kernel_stack)
                         == 4 * Memory::Vmm::page_size,
                     "thread kernel stack spans exactly 4 pages (rsp0 = base + 0x4000)");
        KTEST_ASSERT(t0.kernel_rsp == t0.rsp0,
                     "fresh thread's saved kernel_rsp starts at the stack top");
        KTEST_ASSERT(t0.kernel_stack != t1.kernel_stack,
                     "distinct threads own distinct kernel stacks");

        // ── schedule() runs the only ready thread and points TSS.rsp0 at it ──
        {
            reset_sched();
            Thread::ThreadBlock a {};
            a.m_name = "a";

            KTEST_ASSERT(g_scheduler.curr_thread == nullptr, "fresh scheduler has no current thread");

            g_scheduler.add_ready_threads(&a);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "schedule() picks the only ready thread");
            KTEST_ASSERT(a.state == Thread::ThreadState::Running, "scheduled thread is Running");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 points at the running thread's kernel stack top");
        }

        // ── Round-robin ordering, with TSS.rsp0 following every switch ────────
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a";
            Thread::ThreadBlock b {}; b.m_name = "b";
            Thread::ThreadBlock c {}; c.m_name = "c";

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            g_scheduler.add_ready_threads(&c);

            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "round-robin pick 1 = a");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 = a.rsp0 while a runs");

            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &b, "round-robin pick 2 = b");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0),
                         "TSS.rsp0 follows the switch to b");

            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &c, "round-robin pick 3 = c");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(c.rsp0),
                         "TSS.rsp0 follows the switch to c");

            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "round-robin wraps back to a");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 wraps back to a.rsp0");
        }

        // ── PIT ticks drain the quantum ──────────────────────────────────────
        // On the tick that would reach zero the handler refills the quantum and
        // reschedules, so ticks_left is never observed at 0.
        {
            reset_sched();
            Thread::ThreadBlock a {};
            a.m_name = "a";

            g_scheduler.add_ready_threads(&a);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a,         "current thread is a before any tick");
            KTEST_ASSERT(a.ticks_left == Thread::TIME_SLICE_MS, "fresh thread starts with a full quantum");

            x86::ArchIrq::IrqFrame frame {};
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(a.ticks_left == Thread::TIME_SLICE_MS - 1, "one PIT tick decrements the quantum by one");

            for (std::uint64_t i = 0; i < Thread::TIME_SLICE_MS - 2; ++i) {
                g_scheduler.pit_irq_handler(&frame);
            }
            KTEST_ASSERT(a.ticks_left == 1, "quantum drains down to its final tick");

            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(a.ticks_left == Thread::TIME_SLICE_MS, "expiry refills the quantum, never rests at 0");
            KTEST_ASSERT(g_scheduler.curr_thread == &a,         "the only ready thread keeps running");
        }

        // ── PIT expiry rotates the thread AND moves TSS.rsp0 with it ──────────
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a";
            Thread::ThreadBlock b {}; b.m_name = "b";

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "current thread is a at start of slice");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 = a.rsp0 at start of slice");

            x86::ArchIrq::IrqFrame frame {};
            // TIME_SLICE_MS - 1 ticks bring a's quantum to 1 without expiring.
            for (std::uint64_t i = 0; i < Thread::TIME_SLICE_MS - 1; ++i) {
                g_scheduler.pit_irq_handler(&frame);
            }
            KTEST_ASSERT(a.ticks_left == 1,             "a still holds the CPU on its last tick");
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "no switch before the slice expires");

            // The next tick expires a's slice: refills a and reschedules to b,
            // and that timer-driven switch must move TSS.rsp0 onto b.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &b,         "expired quantum reschedules to b");
            KTEST_ASSERT(a.ticks_left == Thread::TIME_SLICE_MS, "a's quantum is refilled when preempted");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0),
                         "the timer-driven switch updates TSS.rsp0 to b.rsp0");
        }

      
        // Leave the scheduler clean: the blocks above queued stack-local threads
        // that are now out of scope, and the live add_init_thread() path checks
        // ready_threads.empty() to seed curr_thread. Without this, it inherits
        // dangling pointers and the real timer faults on the first tick.
        reset_sched();

        KTest::summary("Scheduler");
    }
}
