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

        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a";
            Thread::ThreadBlock b {}; b.m_name = "b";

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            x86::ArchIrq::IrqFrame frame {};

            for (std::uint64_t i = 0; i < 21; ++i) {
                g_scheduler.pit_irq_handler(&frame);

                if (i == 0) {
                    KTEST_ASSERT(g_scheduler.get_current_thread() == &a, "first thead should be A");
                } else if (i == 1) {
                    KTEST_ASSERT(g_scheduler.get_current_thread() == &b, "first thead should be B"); 
                }
            }

            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(b.ticks_left == Thread::TIME_SLICE_MS, "reset thread B's timer");
            KTEST_ASSERT(&b == g_scheduler.get_current_thread(), "Last thread");        
        }

        // ── Every tick rotates and restores the INCOMING thread's registers ──
        // The handler reschedules on every tick, so each tick saves the outgoing
        // thread's live frame into its TCB and loads the incoming thread's saved
        // frame back into *frame for the ISR epilogue / iretq.
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a";
            Thread::ThreadBlock b {}; b.m_name = "b";

            // b waits, so its context lives in its TCB; a runs, so its live
            // context lives in the frame (the handler reads it from there).
            b.registers.rip = 0xB0;
            b.registers.rax = 0xBB;

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "a runs first");

            x86::ArchIrq::IrqFrame frame {};
            frame.rip = 0xA0;
            frame.rax = 0xAA;

            // One tick rotates a -> b: a's live frame is saved, b's saved frame
            // is restored into *frame.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &b, "one tick rotates to b");
            KTEST_ASSERT(a.registers.rip == 0xA0 && a.registers.rax == 0xAA,
                         "outgoing thread a's live registers are saved into its TCB");
            KTEST_ASSERT(frame.rip == 0xB0 && frame.rax == 0xBB,
                         "incoming thread b's saved registers are restored into the frame");

            // The next tick rotates b -> a: b is saved, a's own registers come
            // back into the frame.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "the next tick rotates back to a");
            KTEST_ASSERT(b.registers.rip == 0xB0 && b.registers.rax == 0xBB,
                         "outgoing thread b's live registers are saved into its TCB");
            KTEST_ASSERT(frame.rip == 0xA0 && frame.rax == 0xAA,
                         "incoming thread a's saved registers are restored into the frame");
        }

        // ── Every tick rotates the thread AND moves TSS.rsp0 with it ──────────
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a";
            Thread::ThreadBlock b {}; b.m_name = "b";

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "current thread is a at start");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 = a.rsp0 while a runs");

            x86::ArchIrq::IrqFrame frame {};

            // Each tick rotates to the next thread and moves TSS.rsp0 with it.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &b, "one tick rotates to b");
            KTEST_ASSERT(a.ticks_left == Thread::TIME_SLICE_MS - 1,
                         "the running thread's quantum ticks down by one");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0),
                         "TSS.rsp0 follows the switch to b");

            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "the next tick rotates back to a");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 follows the switch back to a");
        }

        reset_sched();

        KTest::summary("Scheduler");
    }
}
