#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/arch/cpu.hpp"
#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include "arch/x86-64/system/gdt.hpp"
#include "drivers/pit.hpp"
#include "util/kernel_logger.hpp"
#include <arch/x86-64/proc/scheduler.hpp>
#include <cstddef>
#include <cstdint>
#include "util/ansi.hpp"

namespace x86::Proc::Scheduler {
    void Schedule::lock_scheduler() {
        Cpu::cli();
        disable_counter++;
        task_switch_counter++;
    }

    void Schedule::unlock_scheduler() {
        task_switch_counter--;

        if (task_switch_counter == 0) {
            if (task_switch_postponed) {
                task_switch_postponed = false;
                schedule();
            }
        }

        disable_counter--;
        if (disable_counter == 0) {
            Cpu::sti();
        }
    }

    void Schedule::add_ready_threads(Thread::ThreadBlock* thread) {
        if (ready_threads.empty()) {
            ready_threads.push_head(thread);
            System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(thread->rsp0);
            
            curr_ready_thread = thread;
            curr_thread = thread;

            return;
        }

        ready_threads.push_tail(thread);
    }

    void Schedule::pit_irq_handler(ArchIrq::IrqFrame* frame) {
        if (!curr_thread && !curr_ready_thread) {
            return;
        }

        lock_scheduler();

        curr_thread->registers = *frame;

        if (!sleeping_threads.empty()) {
            auto* sleep_thread = sleeping_threads.pop_head();

            if (sleep_thread->ticks_left != 0) {
                sleep_thread->ticks_left--;
            }

            if (sleep_thread->ticks_left == 0) {
                unblock(sleep_thread);
            }

            sleeping_threads.push_tail(sleep_thread);
        }

        if (curr_thread->ticks_left != 0) {
            if (--curr_thread->ticks_left == 0) {
                curr_thread->ticks_left = Thread::TIME_SLICE_MS;
                Schedule::schedule();
            }
        } 

        unlock_scheduler();
    }

    void Schedule::schedule() {
        if (task_switch_counter) {
            task_switch_postponed = true;
            return;
        }

        if (!ready_threads.empty() && !curr_ready_thread) {
            curr_ready_thread = ready_threads.pop_head();
            curr_ready_thread->state = Thread::ThreadState::Running;
            switch_task(*curr_ready_thread);
        } else if (curr_ready_thread) {
            curr_ready_thread->state = Thread::ThreadState::Ready;
            ready_threads.push_tail(curr_ready_thread);

            curr_ready_thread = ready_threads.pop_head();
            curr_ready_thread->state = Thread::ThreadState::Running;
            switch_task(*curr_ready_thread);
        } 
    }

    void Schedule::switch_task(Thread::ThreadBlock& next_thread) {
        System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(next_thread.rsp0);

        curr_thread = &next_thread;
    }

    void Schedule::sleep(std::uint64_t time) {
        lock_scheduler();

        if (time > Drivers::Pit::get_time_milliseconds()) {
            unlock_scheduler();
            return;
        }

        curr_thread->ticks_left = time;
        curr_thread->state = Thread::ThreadState::Sleep;
        sleeping_threads.push_tail(curr_thread);

        unlock_scheduler();
    }

    void Schedule::yield() {
        lock_scheduler();

        if (curr_thread) {
            curr_thread->state = Thread::ThreadState::Ready;
            ready_threads.push_tail(curr_thread);
        }

        Schedule::schedule();

        unlock_scheduler();
    }

    void Schedule::block() {
        lock_scheduler();

        curr_thread->state = Thread::ThreadState::Sleep;
        sleeping_threads.push_tail(curr_thread);

        schedule();
        unlock_scheduler();
    }

    void Schedule::unblock(Thread::ThreadBlock* task) {
        lock_scheduler();

        if (ready_threads.empty()) {
            Schedule::switch_task(*task);
        }

        task->state = Thread::ThreadState::Ready;
        ready_threads.push_tail(task);
        unlock_scheduler();
    }
}
