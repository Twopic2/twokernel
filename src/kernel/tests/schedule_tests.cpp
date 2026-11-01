#include "memory/vmm.hpp"
#include <tests/ktest.hpp>
#include <arch/x86-64/proc/scheduler.hpp>
#include <arch/x86-64/proc/thread.hpp>
#include <arch/x86-64/proc/process.hpp>
#include <arch/x86-64/system/gdt.hpp>
#include <drivers/pit.hpp>
#include <cstdint>
#include <initializer_list>

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
        // idle_thread is one real, permanently-shared object now (no more
        // per-TU copies once global_idle lost `static`) -- any test that
        // ticks or switches through it mutates its saved registers/
        // runtime accounting/state for real (pit_irq_handler overwrites registers
        // wholesale), and none of that was ever undone. It silently leaked
        // into every test that ran afterward. Capture it clean once, before
        // any test has touched it, and restore from that snapshot every time.
        Thread::ThreadBlock& idle_pristine_snapshot() {
            static Thread::ThreadBlock snapshot = *g_scheduler.idle_thread;
            return snapshot;
        }

        void reset_sched() {
            g_scheduler.ready_threads.clear_all();
            g_scheduler.waiting_queue.clear_all();
            g_scheduler.curr_thread          = nullptr;
            g_scheduler.task_switch_counter  = 0;

            g_scheduler.disable_counter      = 0;
            g_scheduler.task_switch_postponed = false;
            g_scheduler.time_quantum_past     = false;

            *g_scheduler.idle_thread = idle_pristine_snapshot();
        }

        // Every add_ready_threads()/switch_task() crossing leaves CR3 pointing
        // at some process's PML4, so remember the kernel address space on the
        // way in and hand it back on the way out. RAII so a failing REQUIRE
        // (which returns early) still restores CR3 and resets the scheduler.
        struct SchedFixture {
            std::uint64_t kernel_cr3 = Memory::Vmm::read_cr3();
            // The scheduler reads wall time now, so a test that winds the PIT
            // forward would otherwise leak that clock into every later test.
            std::uint64_t saved_clocks = Drivers::Pit::pit_clocks;
            std::uint64_t saved_tick   = Drivers::Pit::tick;
            std::uint64_t saved_seconds = Drivers::Pit::seconds;
            std::uint64_t saved_milliseconds = Drivers::Pit::milli_seconds;
            std::uint64_t saved_rsp0 = x86::System::Gdt::tss.rsp0;
            std::uint64_t saved_syscall_rsp = Cpu::bsp_cpu.syscall_kernel_rsp;

            SchedFixture() {
                reset_sched();
            }
            ~SchedFixture() {
                reset_sched();
                Memory::Vmm::load_cr3(kernel_cr3);
                Drivers::Pit::pit_clocks = saved_clocks;
                Drivers::Pit::tick       = saved_tick;
                Drivers::Pit::seconds = saved_seconds;
                Drivers::Pit::milli_seconds = saved_milliseconds;
                x86::System::Gdt::tss.rsp0 = saved_rsp0;
                Cpu::bsp_cpu.syscall_kernel_rsp = saved_syscall_rsp;
            }
        };

        // isr_timer() advances the PIT before handing off to the scheduler.
        // Driving pit_irq_handler() directly has to do the same or now()
        // never moves and no slice ever expires.
        void tick(x86::ArchIrq::IrqFrame* frame) {
            Drivers::Pit::increase_tick();
            Drivers::Pit::increase_time();
            g_scheduler.pit_irq_handler(frame);
        }

        // The configured 1000 Hz PIT advances the test clock by 1 ms per tick.
        void tick_for_ms(x86::ArchIrq::IrqFrame* frame, std::uint64_t ms) {
            for (std::uint64_t i = 0; i < ms; ++i) {
                tick(frame);
            }
        }

        // Timing tests begin after dispatch: the running thread is outside
        // ready_threads, and the other threads wait in FIFO order. Seeding
        // this state directly also avoids add_ready_threads() resetting the
        // custom budgets that these tests are meant to exercise.
        void start_round_robin(Thread::ThreadBlock& running,
                               std::initializer_list<Thread::ThreadBlock*> ready) {
            running.state = Thread::ThreadState::Running;
            running.exec_start_ms = Drivers::Pit::get_time_milliseconds();
            g_scheduler.curr_thread = &running;
            x86::System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(running.rsp0);
            Cpu::bsp_cpu.syscall_kernel_rsp = x86::System::Gdt::tss.rsp0;
            Memory::Vmm::load_cr3(running.m_process->address_space.page_dir.pml4_pa);

            for (auto* thread : ready) {
                thread->state = Thread::ThreadState::Ready;
                g_scheduler.ready_threads.push_tail(thread);
            }
        }

        struct SliceExpectation {
            Thread::ThreadBlock* running;
            std::uint64_t duration_ms;
            Thread::ThreadBlock* next;
            std::uint64_t remaining_ms;
        };

        void check_slice(x86::ArchIrq::IrqFrame* frame, const SliceExpectation& slice) {
            CHECK(g_scheduler.curr_thread == slice.running);
            const auto runtime_before = slice.running->total_runtime_ms;

            tick_for_ms(frame, slice.duration_ms - 1);
            CHECK(g_scheduler.curr_thread == slice.running);
            CHECK(slice.running->total_runtime_ms == runtime_before + slice.duration_ms - 1);

            tick(frame);
            CHECK(g_scheduler.curr_thread == slice.next);
            CHECK(slice.next->state == Thread::ThreadState::Running);
            CHECK(slice.running->state == Thread::ThreadState::Ready);
            CHECK(slice.running->total_runtime_ms == runtime_before + slice.duration_ms);
            CHECK(slice.running->scheduled_time == slice.remaining_ms);
            CHECK(slice.next->exec_start_ms == Drivers::Pit::get_time_milliseconds());
            CHECK(x86::System::Gdt::tss.rsp0 == reinterpret_cast<std::uint64_t>(slice.next->rsp0));
            CHECK(Cpu::bsp_cpu.syscall_kernel_rsp == x86::System::Gdt::tss.rsp0);
            CHECK_FALSE(g_scheduler.time_quantum_past);
            CHECK_FALSE(g_scheduler.task_switch_postponed);
            CHECK(g_scheduler.task_switch_counter == 0);
            CHECK(g_scheduler.disable_counter == 0);
        }
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

    TEST_CASE("kernel thread entry has an aligned stack and receives its argument", "[scheduler][thread-start]") {
        SchedFixture fixture {};
        int argument = 42;
        Thread::ThreadBlock thread {"probe", &g_kernel_process, thread_a, &argument};
        CHECK(thread.registers.rsp % 16 == 8);
        CHECK(thread.registers.rsp == reinterpret_cast<std::uintptr_t>(thread.rsp0) - 8);
        CHECK(thread.registers.rdi == reinterpret_cast<std::uintptr_t>(&argument));
        CHECK(thread.registers.rip == reinterpret_cast<std::uintptr_t>(thread_a));
        thread.exit();
    }

    TEST_CASE("the first scheduled thread keeps its entry frame until dispatched", "[scheduler][thread-start]") {
        SchedFixture fixture {};
        Schedule scheduler {g_scheduler.idle_thread};
        REQUIRE(scheduler.curr_thread == scheduler.idle_thread);

        Thread::ThreadBlock thread {"probe", &g_kernel_process, thread_a, nullptr};
        const auto entry = thread.registers;
        scheduler.add_ready_threads(&thread);
        CHECK(scheduler.curr_thread == scheduler.idle_thread);

        x86::ArchIrq::IrqFrame frame {};
        for (std::uint64_t i = 0; i < Thread::DEFAULT_SCHEDULE_TIME_MS; ++i) {
            Drivers::Pit::increase_tick();
            Drivers::Pit::increase_time();
            scheduler.pit_irq_handler(&frame);
        }
        CHECK(scheduler.curr_thread == &thread);
        CHECK(thread.state == Thread::ThreadState::Running);
        CHECK(frame.rip == entry.rip);
        CHECK(frame.rsp == entry.rsp);
        CHECK(scheduler.ready_threads.empty());
        thread.exit();
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

    TEST_CASE("each PIT tick charges the running thread one millisecond", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.schedule();
        REQUIRE(g_scheduler.curr_thread == &a);
        REQUIRE(a.scheduled_time == Thread::DEFAULT_SCHEDULE_TIME_MS);
        REQUIRE(a.total_runtime_ms == 0);

        x86::ArchIrq::IrqFrame frame {};
        tick(&frame);
        CHECK(a.total_runtime_ms == 1);

        // A tick is 1ms, so five of them land inside the default slice and
        // the thread is still the one on the cpu.
        tick_for_ms(&frame, 4);
        CHECK(a.total_runtime_ms == 5);
        CHECK(g_scheduler.curr_thread == &a);
    }

    // Rotation is driven by slice expiry, not by the tick itself: a tick that
    // lands mid-slice leaves the running thread alone.
    TEST_CASE("the CPU rotates on slice expiry, not on every tick", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.add_ready_threads(&b);
        g_scheduler.schedule();
        REQUIRE(g_scheduler.curr_thread == &a);

        x86::ArchIrq::IrqFrame frame {};

        tick_for_ms(&frame, Thread::DEFAULT_SCHEDULE_TIME_MS - 1);
        CHECK(g_scheduler.curr_thread == &a);

        tick(&frame);
        CHECK(g_scheduler.curr_thread == &b);
    }

    // A per-thread budget can expire before the scheduler's quantum cap.
    TEST_CASE("a thread with a shorter slice is preempted sooner", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;

        a.update_scheduled_time(3);
        start_round_robin(a, {&b});
        REQUIRE(g_scheduler.curr_thread == &a);

        x86::ArchIrq::IrqFrame frame {};
        check_slice(&frame, {&a, 3, &b, Thread::DEFAULT_SCHEDULE_TIME_MS});
        CHECK(b.total_runtime_ms == 0);
    }

    TEST_CASE("mixed budgets A=6 B=120 C=70 rotate and preserve B's remainder", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};
        Thread::ThreadBlock a {}; a.m_name = "A"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "B"; b.m_process = &kproc;
        Thread::ThreadBlock c {}; c.m_name = "C"; c.m_process = &kproc;

        a.update_scheduled_time(6);
        b.update_scheduled_time(120);
        c.update_scheduled_time(70);
        a.registers.rip = 0xA0;
        b.registers.rip = 0xB0;
        c.registers.rip = 0xC0;
        start_round_robin(a, {&b, &c});

        // At a 100 ms quantum: A(6), B(100), C(70), A(6), B(20).
        // Exhausted budgets reset to the thread default; B keeps its final
        // 20 ms while C and A take their turns.
        const SliceExpectation timeline[] {
            {&a, 6, &b, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&b, DEFAULT_TIME_QUANTUM, &c, 120 - DEFAULT_TIME_QUANTUM},
            {&c, 70, &a, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&a, Thread::DEFAULT_SCHEDULE_TIME_MS, &b, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&b, 120 - DEFAULT_TIME_QUANTUM, &c, Thread::DEFAULT_SCHEDULE_TIME_MS},
        };

        auto frame = a.registers;
        for (const auto& slice : timeline) {
            SECTION(slice.running->m_name.data()) {
                check_slice(&frame, slice);
                CHECK(g_scheduler.ready_threads.size() == 2);
                const auto expected_rip = slice.next == &a ? 0xA0u
                    : slice.next == &b ? 0xB0u : 0xC0u;
                CHECK(frame.rip == expected_rip);
            }
        }

        CHECK(a.total_runtime_ms == 6 + Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(b.total_runtime_ms == 120);
        CHECK(c.total_runtime_ms == 70);
        CHECK(a.scheduled_time == Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(b.scheduled_time == Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(c.scheduled_time == Thread::DEFAULT_SCHEDULE_TIME_MS);
    }

    TEST_CASE("mixed budgets split a 250 ms thread across multiple round-robin turns", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};
        Thread::ThreadBlock a {}; a.m_name = "A"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "B"; b.m_process = &kproc;
        Thread::ThreadBlock c {}; c.m_name = "C"; c.m_process = &kproc;

        a.update_scheduled_time(6);
        b.update_scheduled_time(70);
        c.update_scheduled_time(2 * DEFAULT_TIME_QUANTUM + 50);
        start_round_robin(a, {&b, &c});

        // C runs for 100, 100, then 50 ms, with A and B in between.
        const SliceExpectation timeline[] {
            {&a, 6, &b, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&b, 70, &c, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&c, DEFAULT_TIME_QUANTUM, &a, DEFAULT_TIME_QUANTUM + 50},
            {&a, Thread::DEFAULT_SCHEDULE_TIME_MS, &b, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&b, Thread::DEFAULT_SCHEDULE_TIME_MS, &c, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&c, DEFAULT_TIME_QUANTUM, &a, 50},
            {&a, Thread::DEFAULT_SCHEDULE_TIME_MS, &b, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&b, Thread::DEFAULT_SCHEDULE_TIME_MS, &c, Thread::DEFAULT_SCHEDULE_TIME_MS},
            {&c, 50, &a, Thread::DEFAULT_SCHEDULE_TIME_MS},
        };

        x86::ArchIrq::IrqFrame frame {};
        for (const auto& slice : timeline) {
            SECTION(slice.running->m_name.data()) {
                check_slice(&frame, slice);
                CHECK(g_scheduler.ready_threads.size() == 2);
            }
        }

        CHECK(a.total_runtime_ms == 6 + 2 * Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(b.total_runtime_ms == 70 + 2 * Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(c.total_runtime_ms == 2 * DEFAULT_TIME_QUANTUM + 50);
        CHECK(c.scheduled_time == Thread::DEFAULT_SCHEDULE_TIME_MS);
    }

    TEST_CASE("budgets below, equal to, and above the quantum switch at the correct boundary", "[scheduler]") {
        const struct BudgetCase {
            const char* name;
            std::uint64_t ms;
        } budgets[] {
            {"1 ms", 1}, {"3 ms", 3}, {"6 ms", 6}, {"70 ms", 70},
            {"quantum minus 1 ms", DEFAULT_TIME_QUANTUM - 1},
            {"exactly one quantum", DEFAULT_TIME_QUANTUM},
            {"quantum plus 1 ms", DEFAULT_TIME_QUANTUM + 1},
            {"two quanta plus 50 ms", 2 * DEFAULT_TIME_QUANTUM + 50},
        };

        for (const auto& test : budgets) {
            SchedFixture fixture {};
            Process::ProcessBlock kproc {};
            Thread::ThreadBlock a {}; a.m_name = "A"; a.m_process = &kproc;
            Thread::ThreadBlock b {}; b.m_name = "B"; b.m_process = &kproc;

            const auto budget = test.ms;
            a.update_scheduled_time(budget);
            start_round_robin(a, {&b});
            const std::uint64_t duration = budget < DEFAULT_TIME_QUANTUM ? budget : DEFAULT_TIME_QUANTUM;
            const std::uint64_t remaining = budget > DEFAULT_TIME_QUANTUM
                ? budget - DEFAULT_TIME_QUANTUM : Thread::DEFAULT_SCHEDULE_TIME_MS;

            x86::ArchIrq::IrqFrame frame {};
            SECTION(test.name) {
                check_slice(&frame, {&a, duration, &b, remaining});
                CHECK(b.total_runtime_ms == 0);
                CHECK(g_scheduler.ready_threads.size() == 1);
            }
        }
    }

    TEST_CASE("yield resets the remainder of a quantum-preempted thread", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};
        Thread::ThreadBlock a {}; a.m_name = "A"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "B"; b.m_process = &kproc;

        b.update_scheduled_time(120);
        start_round_robin(b, {&a});

        x86::ArchIrq::IrqFrame frame {};
        check_slice(&frame, {&b, DEFAULT_TIME_QUANTUM, &a, 120 - DEFAULT_TIME_QUANTUM});
        g_scheduler.yield();
        REQUIRE(g_scheduler.curr_thread == &b);
        CHECK(b.scheduled_time == 120 - DEFAULT_TIME_QUANTUM);

        g_scheduler.yield();
        CHECK(g_scheduler.curr_thread == &a);
        CHECK(b.scheduled_time == Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(b.total_runtime_ms == DEFAULT_TIME_QUANTUM);
        CHECK_FALSE(g_scheduler.time_quantum_past);
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

        // Run out a's slice: rotation a -> b.
        tick_for_ms(&frame, Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(a.registers.rip == 0xA0 && a.registers.rax == 0xAA);
        CHECK(frame.rip == 0xB0 && frame.rax == 0xBB);

        // Run out b's slice: rotation b -> a, and a's own frame comes back.
        tick_for_ms(&frame, Thread::DEFAULT_SCHEDULE_TIME_MS);
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

        // Slice expiry rotates to b and moves TSS.rsp0 with it.
        tick_for_ms(&frame, Thread::DEFAULT_SCHEDULE_TIME_MS);
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
        CHECK(g_scheduler.ready_threads.size() == 1);

        g_scheduler.yield();
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(b.state == Thread::ThreadState::Running);
        CHECK(a.state == Thread::ThreadState::Ready);
        CHECK(g_scheduler.ready_threads.size() == 1);

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

    // sleep() stores a countdown in ticks and hands the cpu away immediately.
    TEST_CASE("sleep() parks the caller on the sleeping queue", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Drivers::Pit::tick = 2000;                         // now() == 2000ms

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.sleep(50);

        CHECK(a.state == Thread::ThreadState::Sleep);
        CHECK(a.sleep_time == 50);
        CHECK_FALSE(g_scheduler.waiting_queue.empty());
        // Nothing else was ready, so the cpu went to idle rather than staying
        // with the sleeper.
        CHECK(g_scheduler.curr_thread == g_scheduler.idle_thread);
    }

    // The countdown is a duration, so the clock it was queued at never enters
    // into it -- 50ms is 50 ticks whether uptime reads 2000ms or 0.
    TEST_CASE("sleep() stores a duration, not a point on the clock", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Drivers::Pit::tick = 0;                            // now() == 0

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.sleep(50);
        CHECK(a.state == Thread::ThreadState::Sleep);
        CHECK(a.sleep_time == 50);
        CHECK_FALSE(g_scheduler.waiting_queue.empty());
    }

    // A countdown advances once per tick and the thread is released on exactly
    // the nth one -- and leaves the waiting queue when it goes.
    TEST_CASE("PIT ticks wake a sleeper once its countdown runs out", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock runner  {}; runner.m_name  = "runner";  runner.m_process  = &kproc;
        Thread::ThreadBlock sleeper {}; sleeper.m_name = "sleeper"; sleeper.m_process = &kproc;

        g_scheduler.add_ready_threads(&runner);            // curr = runner

        sleeper.state      = Thread::ThreadState::Sleep;
        sleeper.sleep_time = 3;
        g_scheduler.waiting_queue.push_tail(&sleeper);

        x86::ArchIrq::IrqFrame frame {};
        tick_for_ms(&frame, 2);                            // one tick short
        CHECK(sleeper.state == Thread::ThreadState::Sleep);
        CHECK(sleeper.sleep_time == 1);

        tick(&frame);                                      // the third tick
        CHECK(sleeper.state != Thread::ThreadState::Sleep);
        CHECK(g_scheduler.waiting_queue.empty());
    }

    // Every tick walks the whole queue, so each sleeper runs its own timer and
    // wakes on its own deadline -- queue position buys it nothing.
    TEST_CASE("each sleeper counts down on its own deadline", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock runner {}; runner.m_name = "runner"; runner.m_process = &kproc;
        Thread::ThreadBlock s0 {}; s0.m_name = "s0"; s0.m_process = &kproc;
        Thread::ThreadBlock s1 {}; s1.m_name = "s1"; s1.m_process = &kproc;
        Thread::ThreadBlock s2 {}; s2.m_name = "s2"; s2.m_process = &kproc;

        g_scheduler.add_ready_threads(&runner);

        std::uint64_t ms = 1;
        for (auto* s : {&s0, &s1, &s2}) {
            s->state      = Thread::ThreadState::Sleep;
            s->sleep_time = ms++;
            g_scheduler.waiting_queue.push_tail(s);
        }

        x86::ArchIrq::IrqFrame frame {};

        tick(&frame);
        CHECK(s0.state != Thread::ThreadState::Sleep);
        CHECK(s1.sleep_time == 1);
        CHECK(s2.sleep_time == 2);

        tick(&frame);
        CHECK(s1.state != Thread::ThreadState::Sleep);
        CHECK(s2.state == Thread::ThreadState::Sleep);

        tick(&frame);
        CHECK(s2.state != Thread::ThreadState::Sleep);
        CHECK(g_scheduler.waiting_queue.empty());
    }

    // A woken sleeper must leave waiting_queue on its way to ready_threads:
    // ThreadBlock has exactly one list hook, so it cannot be linked into both
    // -- pushing it onto the second silently truncates the first.
    TEST_CASE("a woken sleeper moves between the queues, it is not copied", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock runner {}; runner.m_name = "runner"; runner.m_process = &kproc;
        Thread::ThreadBlock early {}; early.m_name = "early"; early.m_process = &kproc;
        Thread::ThreadBlock late  {}; late.m_name  = "late";  late.m_process  = &kproc;

        g_scheduler.add_ready_threads(&runner);

        early.state = Thread::ThreadState::Sleep; early.sleep_time = 1;
        late.state  = Thread::ThreadState::Sleep; late.sleep_time  = 4;
        g_scheduler.waiting_queue.push_tail(&early);
        g_scheduler.waiting_queue.push_tail(&late);

        x86::ArchIrq::IrqFrame frame {};
        tick(&frame);

        // early left the waiting queue; late is still on it and still reachable.
        CHECK(g_scheduler.waiting_queue.size() == 1);
        bool late_queued = false;
        for (auto& t : g_scheduler.waiting_queue) {
            CHECK(&t != &early);
            if (&t == &late) late_queued = true;
        }
        CHECK(late_queued);

        bool early_ready = false;
        for (auto& t : g_scheduler.ready_threads) {
            if (&t == &early) early_ready = true;
        }
        CHECK(early_ready);

        // late's timer survived the surgery on the list in front of it.
        tick_for_ms(&frame, 3);
        CHECK(late.state != Thread::ThreadState::Sleep);
        CHECK(g_scheduler.waiting_queue.empty());
    }

    // A short sleep queued behind a long one still lands on time: 2ms behind a
    // 5ms sleep wakes at 2ms, out of queue order, because the two countdowns
    // are independent timers rather than a wake *order*.
    TEST_CASE("a short sleeper queued behind a long one wakes first", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock runner {}; runner.m_name = "runner"; runner.m_process = &kproc;
        Thread::ThreadBlock slow {}; slow.m_name = "slow"; slow.m_process = &kproc;
        Thread::ThreadBlock fast {}; fast.m_name = "fast"; fast.m_process = &kproc;

        g_scheduler.add_ready_threads(&runner);

        slow.state = Thread::ThreadState::Sleep; slow.sleep_time = 5;
        fast.state = Thread::ThreadState::Sleep; fast.sleep_time = 2;
        g_scheduler.waiting_queue.push_tail(&slow);        // head
        g_scheduler.waiting_queue.push_tail(&fast);        // behind it

        x86::ArchIrq::IrqFrame frame {};

        tick_for_ms(&frame, 2);                            // fast's deadline
        CHECK(fast.state != Thread::ThreadState::Sleep);
        // slow was ticked over the same two ticks, from the head.
        CHECK(slow.state == Thread::ThreadState::Sleep);
        CHECK(slow.sleep_time == 3);

        tick_for_ms(&frame, 3);                            // slow's 5th tick
        CHECK(slow.state != Thread::ThreadState::Sleep);
        CHECK(g_scheduler.waiting_queue.empty());
    }

    // The countdown is drained on the way through the handler, not instead of
    // it: the same tick still charges the running thread and still checks its
    // slice, so the cpu keeps rotating while threads sleep.
    TEST_CASE("a countdown tick still rotates the cpu", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;
        Thread::ThreadBlock b {}; b.m_name = "b"; b.m_process = &kproc;
        Thread::ThreadBlock sleeper {}; sleeper.m_name = "sleeper"; sleeper.m_process = &kproc;

        g_scheduler.add_ready_threads(&a);
        g_scheduler.add_ready_threads(&b);
        g_scheduler.schedule();                            // curr = a, ready = [b]
        REQUIRE(g_scheduler.curr_thread == &a);

        sleeper.state      = Thread::ThreadState::Sleep;
        sleeper.sleep_time = 2 * Thread::DEFAULT_SCHEDULE_TIME_MS;
        g_scheduler.waiting_queue.push_tail(&sleeper);

        x86::ArchIrq::IrqFrame frame {};
        tick_for_ms(&frame, Thread::DEFAULT_SCHEDULE_TIME_MS + 2);

        // a's slice expired on tick 6 and b took over, while the sleeper's
        // countdown was drained by all 8 of those ticks.
        CHECK(g_scheduler.curr_thread == &b);
        CHECK(a.total_runtime_ms == Thread::DEFAULT_SCHEDULE_TIME_MS);
        CHECK(b.total_runtime_ms == 2);
        CHECK(sleeper.state == Thread::ThreadState::Sleep);
        CHECK(sleeper.sleep_time == Thread::DEFAULT_SCHEDULE_TIME_MS - 2);

        // The remaining ticks retire the countdown and hand the sleeper to the
        // ready queue rather than leaving it on the waiting queue.
        tick_for_ms(&frame, Thread::DEFAULT_SCHEDULE_TIME_MS - 2);
        CHECK(sleeper.state == Thread::ThreadState::Ready);
        CHECK(g_scheduler.waiting_queue.empty());
    }

    // Blocked threads share the waiting queue but carry no countdown --
    // sleep_time is 0 for them, which the drain would read as "expired". The
    // drain filters on state, so they are stepped over and stay queued until
    // whatever they are waiting on calls unblock().
    TEST_CASE("the sleep drain leaves blocked threads queued", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock runner  {}; runner.m_name  = "runner";  runner.m_process  = &kproc;
        Thread::ThreadBlock blocked {}; blocked.m_name = "blocked"; blocked.m_process = &kproc;

        g_scheduler.add_ready_threads(&runner);

        blocked.state = Thread::ThreadState::Blocked;
        g_scheduler.waiting_queue.push_tail(&blocked);

        x86::ArchIrq::IrqFrame frame {};
        tick_for_ms(&frame, 20);

        CHECK(blocked.state == Thread::ThreadState::Blocked);
        CHECK_FALSE(g_scheduler.waiting_queue.empty());
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
        REQUIRE(pa.address_space.page_dir.pml4_pa != pb.address_space.page_dir.pml4_pa);

        g_scheduler.add_ready_threads(&ta);   // curr = ta, CR3 <- process A
        CHECK((Memory::Vmm::read_cr3() & ~0xFFFull) == (pa.address_space.page_dir.pml4_pa & ~0xFFFull));
        g_scheduler.add_ready_threads(&tb);

        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &ta);

        // Cross into process B: CR3 must follow the address space.
        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &tb);
        CHECK((Memory::Vmm::read_cr3() & ~0xFFFull) == (pb.address_space.page_dir.pml4_pa & ~0xFFFull));

        // Cross back into process A: CR3 swings back.
        g_scheduler.schedule();
        CHECK(g_scheduler.curr_thread == &ta);
        CHECK((Memory::Vmm::read_cr3() & ~0xFFFull) == (pa.address_space.page_dir.pml4_pa & ~0xFFFull));
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

        // Long enough to cross several 10ms slices, not just a few ticks.
        for (int i = 0; i < 40; ++i) {
            tick(&frame);

            if (g_scheduler.curr_thread == &tb) {
                saw_process_b = true;
            }
            const auto live   = Memory::Vmm::read_cr3() & ~0xFFFull;
            const auto wanted = g_scheduler.curr_thread->m_process->address_space.page_dir.pml4_pa & ~0xFFFull;
            if (live != wanted) {
                cr3_tracks_process = false;
            }
        }

        CHECK(saw_process_b);
        CHECK(cr3_tracks_process);
        // process A was charged for the time it spent on the cpu
        CHECK(ta.total_runtime_ms > 0);
    }

    // A single thread that blocks with nothing else ready must hand the CPU
    // to idle -- there's nothing left runnable, so idle is the only valid
    // choice. curr_thread is set directly (not via add_ready_threads, which
    // on an empty queue also leaves the thread sitting in ready_threads --
    // block() would then find ready_threads non-empty and never reach idle).
    TEST_CASE("Checking for idle threads when the ready queue is empty", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.block();

        CHECK(g_scheduler.curr_thread == g_scheduler.idle_thread);
        CHECK(a.state == Thread::ThreadState::Blocked);
        CHECK_FALSE(g_scheduler.waiting_queue.empty());
    }

    // idle is a fallback CPU state, not a schedulable entity -- it must never
    // show up in ready_threads, even after being switched to.
    TEST_CASE("idle is never enqueued in ready_threads", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.block();
        REQUIRE(g_scheduler.curr_thread == g_scheduler.idle_thread);

        for (auto& t : g_scheduler.ready_threads) {
            CHECK(&t != g_scheduler.idle_thread);
        }
    }

    // The blocked thread must not be dropped on the floor when the CPU
    // switches to idle -- it has to stay reachable (in waiting_queue) so
    // unblock() can find it later.
    TEST_CASE("the outgoing thread survives the switch to idle", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;

        g_scheduler.block();
        REQUIRE(g_scheduler.curr_thread == g_scheduler.idle_thread);

        bool found = false;
        for (auto& t : g_scheduler.waiting_queue) {
            if (&t == &a) found = true;
        }
        CHECK(found);
    }

    // Once idle is resident, waking the only other thread must take the CPU
    // back -- idle should never keep running once real work is ready.
    TEST_CASE("unblock() takes the CPU back from idle", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; a.m_name = "a"; a.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;
        g_scheduler.block();
        REQUIRE(g_scheduler.curr_thread == g_scheduler.idle_thread);

        CHECK(g_scheduler.ready_threads.empty() == true);
        g_scheduler.unblock(&a);

        CHECK(g_scheduler.curr_thread == &a);
        CHECK(a.state == Thread::ThreadState::Ready);
    }

    // A PIT tick that fires while idle is already resident and nothing new
    // is ready should leave the CPU parked on idle, not wander off or crash
    // on a redundant self-switch.
    TEST_CASE("PIT ticks while idle is resident are a no-op", "[scheduler]") {
        SchedFixture fixture {};
        Process::ProcessBlock kproc {};

        Thread::ThreadBlock a {}; 
        a.m_name = "a"; 
        a.m_process = &kproc;

        g_scheduler.curr_thread = &a;
        a.state = Thread::ThreadState::Running;
        g_scheduler.block();
        REQUIRE(g_scheduler.curr_thread == g_scheduler.idle_thread);

        x86::ArchIrq::IrqFrame frame {};
        tick_for_ms(&frame, 12);

        CHECK(g_scheduler.curr_thread == g_scheduler.idle_thread);
    }
}
