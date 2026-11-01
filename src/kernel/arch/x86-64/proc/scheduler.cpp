#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/arch/cpu.hpp"
#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include "arch/x86-64/system/gdt.hpp"
#include "drivers/pit.hpp"
#include "memory/vmm.hpp"
#include "util/kernel_logger.hpp"
#include <arch/x86-64/proc/scheduler.hpp>
#include <cstddef>
#include <cstdint>
#include "util/ansi.hpp"

/* 
    ? Notes: 

    ! Most schedulers are premetive Round robin rather than non-premetive such as SJF which causes stravation on dependent resources

 ?   Response time — time from arrival until the job first gets the CPU:
?    Response Time = First Run Time - Arrival Time

    ? Waiting time — total time spent in the ready queue (not running, not blocked):
    ? Waiting Time = Turnaround Time - Burst Time

    * Linux 3.9 man pages states that the default time quantum is around 100

    * 10-9-2026 update
    * So I decided that the default time allocated to each thread is around 4-6ms
    * But at runtime we can decide how long the task will run, each time it context switches it resets backs to default
    * Listed above is what linux has its time quantum as and if it goes over that we don't 
*/


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
        if (ready_threads.empty() && !curr_thread) {
            ready_threads.push_head(thread);
            System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(thread->rsp0);
            Cpu::bsp_cpu.syscall_kernel_rsp = System::Gdt::tss.rsp0; // ! syscall.S reads this, not the TSS
            Memory::Vmm::load_cr3(thread->m_process->address_space.page_dir.pml4_pa);
            
            curr_thread = thread;
            curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();

            return;
        }

        if (time_quantum_past) {
            thread->state = Thread::ThreadState::Ready;
            ready_threads.push_tail(thread);
            time_quantum_past = false;
            return;
        } 

        thread->state = Thread::ThreadState::Ready;
        thread->reset_scheduled_time();
        ready_threads.push_tail(thread);
    }

    void Schedule::pit_irq_handler(ArchIrq::IrqFrame* frame) {
        if (!curr_thread) {
            return;
        }

        lock_scheduler();
        curr_thread->registers = *frame;

        curr_thread->total_runtime_ms += 1;
        for (auto& sleep_thread : waiting_queue) {
            if (sleep_thread.state == Thread::ThreadState::Blocked) {
                continue;
            }

            if (sleep_thread.sleep_time > 0) {
                sleep_thread.sleep_time -= 1;
            }

            if (sleep_thread.sleep_time == 0) {
                unblock(&sleep_thread);
            }
        }

        auto now = Drivers::Pit::get_time_milliseconds();
        auto delta = now - curr_thread->exec_start_ms;

        if (delta >= curr_thread->scheduled_time) {
            Schedule::schedule();
        } else if (delta == DEFAULT_TIME_QUANTUM) {
            time_quantum_past = true;
            Schedule::schedule();
        }

        unlock_scheduler();
        *frame = curr_thread->registers;
    }

    void Schedule::schedule() {
        if (task_switch_counter) {
            task_switch_postponed = true;
            return;
        }

        if (ready_threads.empty()){
            switch_task(idle_thread);
            return;
        }

        if (time_quantum_past) {
            get_current_thread()->scheduled_time = get_current_thread()->scheduled_time - DEFAULT_TIME_QUANTUM;
            
            if (get_current_thread()->scheduled_time == 0) {
                get_current_thread()->reset_scheduled_time();
            }
        }

        auto next_thread = ready_threads.pop_head();
        next_thread->state = Thread::ThreadState::Running;

        switch_task(next_thread);
    }

    void Schedule::switch_task(Thread::ThreadBlock* next_thread) {
        if (curr_thread->state == Thread::ThreadState::Running && curr_thread != idle_thread) { 
            Schedule::add_ready_threads(curr_thread);
            next_thread->state = Thread::ThreadState::Running;
        }

        if (next_thread == idle_thread) {
            curr_thread = next_thread;
            curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();
            return;
        }

        System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(next_thread->rsp0);
        Cpu::bsp_cpu.syscall_kernel_rsp = System::Gdt::tss.rsp0; // ! syscall.S reads this, not the TSS

        if (next_thread->m_process != curr_thread->m_process) {
            Memory::Vmm::load_cr3(next_thread->m_process->address_space.page_dir.pml4_pa);
        }

        curr_thread = next_thread;
        curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();
    }

    void Schedule::sleep(std::uint64_t milli) {
        lock_scheduler();

        curr_thread->sleep_time = milli;
        curr_thread->state = Thread::ThreadState::Sleep;
        waiting_queue.push_tail(curr_thread);

        schedule();

        unlock_scheduler();
    }

    void Schedule::yield() {
        lock_scheduler();

        curr_thread->state = Thread::ThreadState::Ready;
        curr_thread->reset_scheduled_time();
        ready_threads.push_tail(curr_thread);

        Schedule::schedule();

        unlock_scheduler();
    }

    void Schedule::block() {
        lock_scheduler();

        curr_thread->state = Thread::ThreadState::Blocked;
        waiting_queue.push_tail(curr_thread);

        schedule();
        unlock_scheduler();
    }

    void Schedule::unblock(Thread::ThreadBlock* task) {
        lock_scheduler();

        waiting_queue.remove(task);
        task->sleep_time = 0;
        task->state = Thread::ThreadState::Ready;

        if (ready_threads.empty() && curr_thread == idle_thread) {
            Schedule::switch_task(task);
        } else {
            ready_threads.push_tail(task);
        }

        unlock_scheduler();
    }
}
