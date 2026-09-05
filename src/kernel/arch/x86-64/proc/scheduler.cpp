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

    Example
    A=25ms, B=10ms, C=20ms, all arrive t=0, quantum=10ms
    t=0  → 10:  A runs  (A remaining: 15)
    t=10 → 20:  B runs  (B remaining: 0) → B done at t=20
    t=20 → 30:  C runs  (C remaining: 10)
    t=30 → 40:  A runs  (A remaining: 5)
    t=40 → 50:  C runs  (C remaining: 0) → C done at t=50
    t=50 → 55:  A runs  (A remaining: 0) → A done at t=55
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
        if (ready_threads.empty()) {
            ready_threads.push_head(thread);
            System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(thread->rsp0);
            Cpu::bsp_cpu.syscall_kernel_rsp = System::Gdt::tss.rsp0; // ! syscall.S reads this, not the TSS
            Memory::Vmm::load_cr3(thread->m_process->vaddr.pml4_pa);
            
            curr_thread = thread;
            curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();

            return;
        }

        thread->state = Thread::ThreadState::Ready;
        ready_threads.push_tail(thread);
    }

    void Schedule::pit_irq_handler(ArchIrq::IrqFrame* frame) {
        if (!curr_thread) {
            return;
        }

        lock_scheduler();
        curr_thread->registers = *frame;

        // TODO This is a massive problem MUSTFIX
        if (!waiting_queue.empty()) {
            auto* sleep_thread = waiting_queue.pop_head();

            if (sleep_thread->state == Thread::ThreadState::Blocked) {
                waiting_queue.push_head(sleep_thread);
                
                unlock_scheduler();
                *frame = curr_thread->registers;

                return;
            }

            if (sleep_thread->sleep_time > 0) {
                --sleep_thread->sleep_time;
            } 

            if (sleep_thread->sleep_time != 0) {
                waiting_queue.push_head(sleep_thread);
            } else {
                unblock(sleep_thread);
            } 

            unlock_scheduler();
            *frame = curr_thread->registers;

            return;
        }
        
        auto now = Drivers::Pit::get_time_milliseconds();
        auto delta = now - curr_thread->exec_start_ms;
      
        curr_thread->total_runtime_ms += 1;

        if (delta >= curr_thread->scheduled_time) {
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

        auto next_thread = ready_threads.pop_head();
        next_thread->state = Thread::ThreadState::Running;

        switch_task(next_thread);
    }

    void Schedule::switch_task(Thread::ThreadBlock* next_thread) {
        //Util::dump_registers("current thread before context switch", curr_thread->registers); 
        if (curr_thread->m_process->is_dead && !next_thread->m_process->is_dead) {
            Memory::Vmm::load_cr3(next_thread->m_process->vaddr.pml4_pa);
            curr_thread = next_thread;
            curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();
            return;
        } 
        
        if (curr_thread->state == Thread::ThreadState::Running && curr_thread != idle_thread) { 
            Schedule::add_ready_threads(curr_thread);
        }

        if (next_thread->m_process->is_dead) {
            schedule();
            return;
        }

        if (next_thread == idle_thread) {
            curr_thread = next_thread;
            curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();
            return;
        }

        System::Gdt::tss.rsp0 = reinterpret_cast<std::uint64_t>(next_thread->rsp0);
        Cpu::bsp_cpu.syscall_kernel_rsp = System::Gdt::tss.rsp0; // ! syscall.S reads this, not the TSS

        if (next_thread->m_process != curr_thread->m_process) {
            Memory::Vmm::load_cr3(next_thread->m_process->vaddr.pml4_pa);
        }

        curr_thread = next_thread;
        curr_thread->exec_start_ms = Drivers::Pit::get_time_milliseconds();
      //  Util::dump_registers("next thread after context switch", next_thread.registers);
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
        curr_thread->reset();
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

    Thread::ThreadBlock* Schedule::get_current_thread() {
        Util::IrqGaurd irq_gaurd {};
        return curr_thread;
    }

    void Schedule::unblock(Thread::ThreadBlock* task) {
        lock_scheduler();

        if (ready_threads.empty()) {
            Schedule::switch_task(task);
        }

        task->state = Thread::ThreadState::Ready;
        ready_threads.push_tail(task);
        unlock_scheduler();
    }
}
