#include "memory/vmm.hpp"
#include <tests/ktest.hpp>
#include <arch/x86-64/proc/scheduler.hpp>
#include <arch/x86-64/proc/thread.hpp>
#include <arch/x86-64/proc/process.hpp>
#include <arch/x86-64/system/gdt.hpp>
#include <drivers/pit.hpp>
#include <cstdint>

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

    namespace {
        void reset_sched() {
            g_scheduler.ready_threads.clear_all();
            g_scheduler.sleeping_threads.clear_all();
            g_scheduler.curr_thread          = nullptr;
            g_scheduler.task_switch_counter  = 0;

            g_scheduler.disable_counter      = 0;
            g_scheduler.task_switch_postponed = false;
        }

        // Every add_ready_threads()/switch_task() crossing leaves CR3 pointing
        // at some process's PML4, so remember the kernel address space on the
        // way in and hand it back on the way out. RAII so a failing REQUIRE
        // (which returns early) still restores CR3 and resets the scheduler.
        struct SchedFixture {
            std::uint64_t kernel_cr3 = Memory::Vmm::read_cr3();

            SchedFixture() {
                reset_sched();
            }
            ~SchedFixture() {
                reset_sched();
                Memory::Vmm::load_cr3(kernel_cr3);
            }
        };
    }

    TEST_CASE("a thread acquires its own kernel stack", "[scheduler]") {
        Thread::ThreadBlock t0 {};
        Thread::ThreadBlock t1 {};

        // stack base (kernel_stack) sits below its top (rsp0)
        CHECK(t0.kernel_stack != t0.rsp0);
        // spans exactly 4 pages (rsp0 = base + 0x4000)
        CHECK(reinterpret_cast<std::uintptr_t>(t0.rsp0) - reinterpret_cast<std::uintptr_t>(t0.kernel_stack)
                  == 4 * Memory::Vmm::PAGE_SIZE);
        // distinct threads own distinct kernel stacks
        CHECK(t0.kernel_stack != t1.kernel_stack);
    }

    TEST_CASE("schedule() runs the only ready thread and points TSS.rsp0 at it", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        REQUIRE(g_scheduler.curr_thread == nullptr);

        g_scheduler.add_ready_threads(&a);
        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &a);
        CHECK(a.state == Thread::ThreadState::Running);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0));
    }

    TEST_CASE("round-robin ordering, with TSS.rsp0 following every switch", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;
        Thread::ThreadBlock c {}; c.m_name = "c"; c.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.add_ready_threads(&b);
        g_scheduler.add_ready_threads(&c);

        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &a);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0));

        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0));

        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &c);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(c.rsp0));

        // wraps back around
        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &a);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0));
    }

    // On the tick that would reach zero the handler refills the quantum and
    // reschedules, so ticks_left is never observed at 0.
    TEST_CASE("PIT ticks drain the quantum and expiry refills it", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.schedule();
        REQUIRE(g_scheduler.curr_thread == &a);
        REQUIRE(a.ticks_left == Thread::TIME_SLICE_MS);

        x86::ArchIrq::IrqFrame frame {};
        g_scheduler.pit_irq_handler(&frame);
        CHECK(a.ticks_left == Thread::TIME_SLICE_MS - 1);

        for (std::uint64_t i = 0; i < Thread::TIME_SLICE_MS - 2; ++i) {
            g_scheduler.pit_irq_handler(&frame);
        }
        CHECK(a.ticks_left == 1);

        g_scheduler.pit_irq_handler(&frame);
        CHECK(a.ticks_left == Thread::TIME_SLICE_MS);
        CHECK(g_scheduler.curr_thread == &a);
    }

    // The handler reschedules on every tick, so with a second thread ready, a
    // single tick hands the CPU over to b — long before a's quantum drains.
    TEST_CASE("every PIT tick rotates the CPU", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.add_ready_threads(&b);
        g_scheduler.schedule();
        REQUIRE(g_scheduler.curr_thread == &a);

        x86::ArchIrq::IrqFrame frame {};

        // One tick is enough to rotate now.
        g_scheduler.pit_irq_handler(&frame);
        CHECK(g_scheduler.curr_thread == &b);
        // rotation happens with the quantum barely touched, not on expiry
        CHECK(a.ticks_left == Thread::TIME_SLICE_MS - 1);
    }

    // Each tick saves the outgoing thread's live frame into its TCB and loads
    // the incoming thread's saved frame into *frame for the ISR epilogue.
    TEST_CASE("rotation saves the outgoing frame and restores the incoming one", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

        // b waits, so its context lives in its TCB; a runs, so its live
        // context lives in the frame (the handler reads it from there).
        b.registers.rip = 0xB0;
        b.registers.rax = 0xBB;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.add_ready_threads(&b);
        g_scheduler.schedule();
        REQUIRE(g_scheduler.curr_thread == &a);

        x86::ArchIrq::IrqFrame frame {};
        frame.rip = 0xA0;
        frame.rax = 0xAA;

        // One tick: rotation a -> b.
        g_scheduler.pit_irq_handler(&frame);
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(a.registers.rip == 0xA0 && a.registers.rax == 0xAA);
        CHECK(frame.rip == 0xB0 && frame.rax == 0xBB);

        // Next tick: rotation b -> a, and a's own frame comes back.
        g_scheduler.pit_irq_handler(&frame);
        CHECK(g_scheduler.curr_thread == &a);
        CHECK(b.registers.rip == 0xB0 && b.registers.rax == 0xBB);
        CHECK(frame.rip == 0xA0 && frame.rax == 0xAA);
    }

    TEST_CASE("rotation moves TSS.rsp0 to the incoming thread's stack", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.add_ready_threads(&b);
        g_scheduler.schedule();
        REQUIRE(g_scheduler.curr_thread == &a);
        REQUIRE(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(a.rsp0));

        x86::ArchIrq::IrqFrame frame {};

        // A single tick rotates to b and moves TSS.rsp0 with it.
        g_scheduler.pit_irq_handler(&frame);
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0));
    }

    // a runs (curr_thread, NOT parked in the ready queue) and b waits. A
    // yield should requeue a as Ready and resume b.
    TEST_CASE("yield() hands the CPU to the next ready thread", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;
        b.state = Thread::ThreadState::Ready;
        g_scheduler.ready_threads.push_tail(&b);

        g_scheduler.yield();
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(b.state == Thread::ThreadState::Running);
        CHECK(a.state == Thread::ThreadState::Ready);
        CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(b.rsp0));

        // The lock/postpone dance must net out: yield returns fully unlocked
        // and consumes the postponed switch it queued.
        CHECK(g_scheduler.task_switch_counter == 0 && g_scheduler.disable_counter == 0);
        CHECK_FALSE(g_scheduler.task_switch_postponed);
    }

    TEST_CASE("yield() with nothing else ready keeps the caller running", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.yield();
        CHECK(g_scheduler.curr_thread == &a);
        CHECK(a.state == Thread::ThreadState::Running);
    }

    // The guard compares the requested ticks against the current PIT time,
    // so we push the clock forward to let the request through.
    TEST_CASE("sleep() parks the caller on the sleeping queue", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        const auto saved_clocks = Drivers::Pit::pit_clocks;
        Drivers::Pit::pit_clocks = 2 * Drivers::Pit::HZ;   // now() == 2000 ms

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.sleep(50);
        CHECK(a.state == Thread::ThreadState::Sleep);
        CHECK(a.ticks_left == 50);
        CHECK_FALSE(g_scheduler.sleeping_threads.empty());
        // NOTE: sleep() does not call schedule(), so the sleeper is still the
        // current thread and keeps running until the next PIT tick rotates
        // it out. Worth revisiting if you want sleep() to yield immediately.
        CHECK(g_scheduler.curr_thread == &a);

        Drivers::Pit::pit_clocks = saved_clocks;
    }

    // With the clock at 0, `time > now` is true for any positive sleep, so
    // the guard bails early. This pins the present behaviour of an
    // inverted-looking guard — flip the assertions if you fix it.
    TEST_CASE("sleep() request is dropped when ticks > now (current guard)", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        const auto saved_clocks = Drivers::Pit::pit_clocks;
        Drivers::Pit::pit_clocks = 0;                      // now() == 0 ms

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.sleep(50);                             // 50 > 0 -> early return
        CHECK(a.state == Thread::ThreadState::Running);
        CHECK(g_scheduler.sleeping_threads.empty());

        Drivers::Pit::pit_clocks = saved_clocks;
    }

    // A runner keeps the scheduler live; the sleeper's ticks_left counts
    // down one per tick and the thread is released when it hits zero.
    TEST_CASE("PIT ticks drain a sleeper's timer and wake it", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock runner  {}; runner.m_name  = "runner";  runner.m_process  = &kproc;
        Thread::ThreadBlock sleeper {}; sleeper.m_name = "sleeper"; sleeper.m_process = &kproc;

        g_scheduler.add_ready_threads(&runner);            // curr = runner

        sleeper.state      = Thread::ThreadState::Sleep;
        sleeper.ticks_left = 3;
        g_scheduler.sleeping_threads.push_tail(&sleeper);

        x86::ArchIrq::IrqFrame frame {};
        g_scheduler.pit_irq_handler(&frame);
        CHECK(sleeper.ticks_left == 2);

        g_scheduler.pit_irq_handler(&frame);               // 2 -> 1
        g_scheduler.pit_irq_handler(&frame);               // 1 -> 0, wakes
        CHECK(sleeper.state != Thread::ThreadState::Sleep);
        CHECK_FALSE(g_scheduler.ready_threads.empty());
    }

    // process A owns thread A, process B owns thread B. Each ProcessBlock{}
    // builds a real, kernel-mapped address space, so crossing from one to
    // the other reloads CR3 with the incoming process's PML4.
    //
    // add_ready_threads() loads process A's CR3 as soon as ta becomes the
    // head, and it keeps ta in the ready queue as well as in curr_thread, so
    // the *first* schedule() re-selects process A (no reload). The crossings
    // begin on the next pick.
    TEST_CASE("switching between threads in different processes reloads CR3", "[scheduler]") {
        SchedFixture fixture {};

        Process::ProcessBlock pa {};
        Process::ProcessBlock pb {};

        Thread::ThreadBlock ta {}; ta.m_name = "A/threadA";
        Thread::ThreadBlock tb {}; tb.m_name = "B/threadB";
        pa.add_thread(&ta);                 // ta.m_process = &pa
        pb.add_thread(&tb);                 // tb.m_process = &pb

        REQUIRE(ta.m_process == &pa && tb.m_process == &pb);
        REQUIRE(pa.vaddr.pml4_pa != pb.vaddr.pml4_pa);

        g_scheduler.add_ready_threads(&ta);   // curr = ta, CR3 <- process A
        CHECK((Memory::Vmm::read_cr3() & ~0xFFFull) == (pa.vaddr.pml4_pa & ~0xFFFull));
        g_scheduler.add_ready_threads(&tb);

        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &ta);

        // Cross into process B: CR3 must follow the address space.
        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &tb);
        CHECK((Memory::Vmm::read_cr3() & ~0xFFFull) == (pb.vaddr.pml4_pa & ~0xFFFull));

        // Cross back into process A: CR3 swings back.
        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &ta);
        CHECK((Memory::Vmm::read_cr3() & ~0xFFFull) == (pa.vaddr.pml4_pa & ~0xFFFull));
    }

    // Every tick reschedules, so the CPU rotates between process A and
    // process B. After each tick CR3 must match whatever process the
    // current thread belongs to.
    TEST_CASE("PIT ticks drive the cross-process switch, CR3 tracking curr", "[scheduler]") {
        SchedFixture fixture {};

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

        CHECK(saw_process_b);
        CHECK(cr3_tracks_process);
        // process A's quantum is being spent across the ticks
        CHECK(ta.ticks_left < Thread::TIME_SLICE_MS);
    }
}
