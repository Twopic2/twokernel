#include "memory/vmm.hpp"
#include <tests/ktest.hpp>
#include <arch/x86-64/proc/scheduler.hpp>
#include <arch/x86-64/proc/thread.hpp>
#include <arch/x86-64/proc/process.hpp>
#include <arch/x86-64/system/gdt.hpp>
#include <drivers/pit.hpp>
#include <cstdint>

/*

    ! Maybe I shouldn't vibe code the test cases

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

        // The kernel address space we must hand back when the suite finishes;
        // every add_ready_threads()/switch_task() crossing leaves CR3 pointing
        // at some process's PML4 instead.
        const std::uintptr_t kernel_cr3 = Memory::Vmm::read_cr3();

        auto reset_sched = [] {
            g_scheduler.ready_threads.clear_all();
            g_scheduler.sleeping_threads.clear_all();
            g_scheduler.curr_thread          = nullptr;
            g_scheduler.task_switch_counter  = 0;

            g_scheduler.disable_counter      = 0;
            g_scheduler.task_switch_postponed = false;
        };

        // One shared process to back every single-process test below. Because
        // all its threads share this address space, rotations between them never
        // reload CR3 (switch_task only swaps CR3 across a process boundary).
        Process::ProcessBlock kproc {};

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
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

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
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;
            Thread::ThreadBlock c {}; c.m_name = "c"; c.m_process = &kproc;

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
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

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

        // ── Every PIT tick rotates the CPU (one switch per tick) ─────────────
        // The handler now reschedules on every tick, so with a second thread
        // ready, a single tick hands the CPU over to b — long before a's
        // quantum drains.
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "a runs first");

            x86::ArchIrq::IrqFrame frame {};

            // One tick is enough to rotate now.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &b, "a single tick rotates a -> b");
            KTEST_ASSERT(a.ticks_left == Thread::TIME_SLICE_MS - 1,
                         "rotation happens with the quantum barely touched, not on expiry");
        }

        // ── On rotation the outgoing frame is saved and the incoming restored ──
        // Rotation now fires on every tick: each tick saves the outgoing
        // thread's live frame into its TCB and loads the incoming thread's
        // saved frame into *frame for the ISR epilogue / iretq.
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

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

            // One tick: rotation a -> b.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &b, "one tick rotates a -> b");
            KTEST_ASSERT(a.registers.rip == 0xA0 && a.registers.rax == 0xAA,
                         "outgoing thread a's live registers are saved into its TCB");
            KTEST_ASSERT(frame.rip == 0xB0 && frame.rax == 0xBB,
                         "incoming thread b's saved registers are restored into the frame");

            // Next tick: rotation b -> a, and a's own frame comes back.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "the next tick rotates back to a");
            KTEST_ASSERT(b.registers.rip == 0xB0 && b.registers.rax == 0xBB,
                         "outgoing thread b's live registers are saved into its TCB");
            KTEST_ASSERT(frame.rip == 0xA0 && frame.rax == 0xAA,
                         "incoming thread a's saved registers are restored into the frame");
        }

        // ── On rotation TSS.rsp0 moves to the incoming thread's stack ─────────
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

            g_scheduler.add_ready_threads(&a);
            g_scheduler.add_ready_threads(&b);
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &a, "current thread is a at start");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0),
                         "TSS.rsp0 = a.rsp0 while a runs");

            x86::ArchIrq::IrqFrame frame {};

            // A single tick rotates to b and moves TSS.rsp0 with it.
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(g_scheduler.curr_thread == &b, "a single tick rotates to b");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0),
                         "TSS.rsp0 follows the switch to b");
        }

        // ── yield() hands the CPU to the next ready thread ───────────────────
        // a runs (curr_thread, NOT parked in the ready queue) and b waits. A
        // yield should requeue a as Ready and resume b.
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

            g_scheduler.curr_thread = &a;
            a.state = Thread::ThreadState::Running;
            b.state = Thread::ThreadState::Ready;
            g_scheduler.ready_threads.push_tail(&b);

            g_scheduler.yield();
            KTEST_ASSERT(g_scheduler.curr_thread == &b,
                         "yield() hands the CPU to the next ready thread");
            KTEST_ASSERT(b.state == Thread::ThreadState::Running,
                         "the resumed thread is marked Running");
            KTEST_ASSERT(a.state == Thread::ThreadState::Ready,
                         "the yielding thread is marked Ready");
            KTEST_ASSERT(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0),
                         "TSS.rsp0 follows the yield over to b");

            // The lock/postpone dance must net out: yield returns fully unlocked.
            KTEST_ASSERT(g_scheduler.task_switch_counter == 0 && g_scheduler.disable_counter == 0,
                         "yield() leaves the scheduler unlocked");
            KTEST_ASSERT(!g_scheduler.task_switch_postponed,
                         "yield() consumes the postponed switch it queued");
        }

        // ── yield() with nothing else ready keeps the caller running ─────────
        {
            reset_sched();
            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

            g_scheduler.curr_thread = &a;
            a.state = Thread::ThreadState::Running;

            g_scheduler.yield();
            KTEST_ASSERT(g_scheduler.curr_thread == &a,
                         "yield() with no other ready thread keeps the caller running");
            KTEST_ASSERT(a.state == Thread::ThreadState::Running,
                         "the sole thread stays Running after yield()");
        }

        // ── sleep() parks the caller on the sleeping queue ───────────────────
        // The guard compares the requested ticks against the current PIT time,
        // so we push the clock forward to let the request through.
        {
            reset_sched();
            const auto saved_clocks = Drivers::Pit::pit_clocks;
            Drivers::Pit::pit_clocks = 2 * Drivers::Pit::HZ;   // now() == 2000 ms

            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            g_scheduler.curr_thread = &a;
            a.state = Thread::ThreadState::Running;

            g_scheduler.sleep(50);
            KTEST_ASSERT(a.state == Thread::ThreadState::Sleep,
                         "sleep() marks the caller Sleeping");
            KTEST_ASSERT(a.ticks_left == 50,
                         "sleep() records the requested tick count");
            KTEST_ASSERT(!g_scheduler.sleeping_threads.empty(),
                         "the sleeper is parked on the sleeping queue");
            // NOTE: sleep() does not call schedule(), so the sleeper is still the
            // current thread and keeps running until the next PIT tick rotates
            // it out. Worth revisiting if you want sleep() to yield immediately.
            KTEST_ASSERT(g_scheduler.curr_thread == &a,
                         "sleep() currently does NOT switch away from the caller");

            Drivers::Pit::pit_clocks = saved_clocks;
        }

        // ── sleep() request is dropped when ticks > now (current guard) ──────
        // With the clock at 0, `time > now` is true for any positive sleep, so
        // the guard bails early. This pins the present behaviour of an
        // inverted-looking guard — flip the assertions if you fix it.
        {
            reset_sched();
            const auto saved_clocks = Drivers::Pit::pit_clocks;
            Drivers::Pit::pit_clocks = 0;                      // now() == 0 ms

            Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
            g_scheduler.curr_thread = &a;
            a.state = Thread::ThreadState::Running;

            g_scheduler.sleep(50);                             // 50 > 0 -> early return
            KTEST_ASSERT(a.state == Thread::ThreadState::Running,
                         "sleep(t) with t > now is dropped, caller stays Running");
            KTEST_ASSERT(g_scheduler.sleeping_threads.empty(),
                         "nothing is parked when the sleep guard bails");

            Drivers::Pit::pit_clocks = saved_clocks;
        }

        // ── PIT ticks drain a sleeper's timer and wake it ────────────────────
        // A runner keeps the scheduler live; the sleeper's ticks_left counts
        // down one per tick and the thread is released when it hits zero.
        {
            reset_sched();
            Thread::ThreadBlock runner  {}; runner.m_name  = "runner";  runner.m_process  = &kproc;
            Thread::ThreadBlock sleeper {}; sleeper.m_name = "sleeper"; sleeper.m_process = &kproc;

            g_scheduler.add_ready_threads(&runner);            // curr = runner

            sleeper.state      = Thread::ThreadState::Sleep;
            sleeper.ticks_left = 3;
            g_scheduler.sleeping_threads.push_tail(&sleeper);

            x86::ArchIrq::IrqFrame frame {};
            g_scheduler.pit_irq_handler(&frame);
            KTEST_ASSERT(sleeper.ticks_left == 2,
                         "one PIT tick decrements the sleeper's remaining time");

            g_scheduler.pit_irq_handler(&frame);               // 2 -> 1
            g_scheduler.pit_irq_handler(&frame);               // 1 -> 0, wakes
            KTEST_ASSERT(sleeper.state != Thread::ThreadState::Sleep,
                         "draining the sleep timer wakes the thread");
            KTEST_ASSERT(!g_scheduler.ready_threads.empty(),
                         "the woken sleeper is back on the ready queue");
        }

        // ── Switching between threads that live in different processes ────────
        // process A owns thread A, process B owns thread B. Each ProcessBlock{}
        // builds a real, kernel-mapped address space, so crossing from one to
        // the other reloads CR3 with the incoming process's PML4.
        //
        // add_ready_threads() loads process A's CR3 as soon as ta becomes the
        // head, and it keeps ta in the ready queue as well as in curr_thread, so
        // the *first* schedule() re-selects process A (no reload). The crossings
        // begin on the next pick.
        {
            reset_sched();

            Process::ProcessBlock pa {};
            Process::ProcessBlock pb {};

            Thread::ThreadBlock ta {}; ta.m_name = "A/threadA";
            Thread::ThreadBlock tb {}; tb.m_name = "B/threadB";
            pa.add_thread(&ta);                 // ta.m_process = &pa
            pb.add_thread(&tb);                 // tb.m_process = &pb

            KTEST_ASSERT(ta.m_process == &pa && tb.m_process == &pb,
                         "each thread is bound to its own process");
            KTEST_ASSERT(pa.vaddr.pml4_pa != pb.vaddr.pml4_pa,
                         "the two processes own distinct address spaces");

            g_scheduler.add_ready_threads(&ta);   // curr = ta, CR3 <- process A
            KTEST_ASSERT((Memory::Vmm::read_cr3() & ~0xFFFull) == (pa.vaddr.pml4_pa & ~0xFFFull),
                         "add_ready_threads() loads the first thread's process CR3");
            g_scheduler.add_ready_threads(&tb);

            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &ta,
                         "first schedule() stays on process A's thread");

            // Cross into process B: CR3 must follow the address space.
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &tb,
                         "schedule() crosses over to process B's thread");
            KTEST_ASSERT((Memory::Vmm::read_cr3() & ~0xFFFull) == (pb.vaddr.pml4_pa & ~0xFFFull),
                         "crossing into process B loads B's PML4 into CR3");

            // Cross back into process A: CR3 swings back.
            g_scheduler.schedule();
            KTEST_ASSERT(g_scheduler.curr_thread == &ta,
                         "schedule() crosses back to process A's thread");
            KTEST_ASSERT((Memory::Vmm::read_cr3() & ~0xFFFull) == (pa.vaddr.pml4_pa & ~0xFFFull),
                         "crossing back into process A reloads A's PML4 into CR3");
        }

        // ── PIT ticks drive the cross-process switch, CR3 tracking curr ──────
        // Every tick reschedules, so the CPU rotates between process A and
        // process B. After each tick CR3 must match whatever process the
        // current thread belongs to.
        {
            reset_sched();

            Process::ProcessBlock pa {};
            Process::ProcessBlock pb {};

            Thread::ThreadBlock ta {}; ta.m_name = "A/threadA";
            Thread::ThreadBlock tb {}; tb.m_name = "B/threadB";
            pa.add_thread(&ta);
            pb.add_thread(&tb);

            g_scheduler.add_ready_threads(&ta);   // curr = ta, CR3 <- process A
            g_scheduler.add_ready_threads(&tb);

            x86::ArchIrq::IrqFrame frame {};
            bool saw_process_b = false;
            bool cr3_tracks_process = true;

            for (int i = 0; i < 8; ++i) {
                g_scheduler.pit_irq_handler(&frame);

                if (g_scheduler.curr_thread == &tb) {
                    saw_process_b = true;
                }
                const auto live   = Memory::Vmm::read_cr3() & ~0xFFFull;
                const auto wanted = g_scheduler.curr_thread->m_process->vaddr.pml4_pa & ~0xFFFull;
                if (live != wanted) {
                    cr3_tracks_process = false;
                }
            }

            KTEST_ASSERT(saw_process_b,
                         "PIT ticks rotate the CPU over to process B");
            KTEST_ASSERT(cr3_tracks_process,
                         "after every tick CR3 matches the running thread's process");
            KTEST_ASSERT(ta.ticks_left < Thread::TIME_SLICE_MS,
                         "process A's quantum is being spent across the ticks");
        }

        reset_sched();

        // Hand the kernel address space back; the crossings above left CR3
        // pointing at one of the test processes' page tables.
        Memory::Vmm::load_cr3(kernel_cr3);

        KTest::summary("Scheduler");
    }
}
