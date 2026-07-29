#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include "memory/vmm.hpp"
#include <cstddef>
#include <cstdint>

/// TODO: When I manage to set up SMP. The Cpu will handle scheduling rather than the Kernel. 
/// TODO: Maybe add a idle process list 

/// Some of linux's scheduling algo 
/// SCHED_FIFO, SCHED_RR, SCHED_DEADLINE and SCHED_OTHER

/* 
    !idle thread
    each cpu should only have one idle thread and when the ready_queue is empty 
    a context switch is performed
        
*/

/* 
    ! Proc/Thread state notes

    ? Yild
    yield() keeps the thread runnable. It drops to the back of the runqueue and gets picked again shortly

    ? Blocked
    it sits in a wait queue with a live saved context and holds everything. Meant for disk io waiting
    keyboard, disk, mutex, timer

    ? Zombie
    exited; status retained until parent wait()s

    ? Dead
    reaped; stack + struct pending free by next thread

    ? Sleep
    timed sleep
*/

/* 
    ! TODO
    ! Should impl prioty scheduler or something equal to that nature
 */

namespace x86::Proc::Scheduler {
    inline void set_idle(void*) {
        while (true) {
            asm volatile("hlt");
        }
    }

    struct Schedule {
        std::uint8_t disable_counter {}; 
        std::uint8_t task_switch_counter {};   
        bool task_switch_postponed {};

        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::hook> waiting_queue;
        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::hook> ready_threads;
        Thread::ThreadBlock* curr_thread {};
        Thread::ThreadBlock* idle_thread {};

        void lock_scheduler();
        void unlock_scheduler(); 
        void block();
        void unblock(Thread::ThreadBlock* task);       
        Thread::ThreadBlock* get_current_thread(); 
        void add_ready_threads(Thread::ThreadBlock* thread);
        void switch_task(Thread::ThreadBlock* next_thread);

        void yield();
        void sleep(std::uint64_t ns);

        void pit_irq_handler(ArchIrq::IrqFrame* frame);
        void schedule();
        void reschedule();
        
        Schedule(Thread::ThreadBlock* idle) {
            idle->state = Thread::ThreadState::Running;
            idle_thread = idle;
        }
    };

    inline Process::ProcessBlock global_process {"kernel", false};
    inline Thread::ThreadBlock global_idle  {"idle", &global_process, set_idle, nullptr};
    inline struct Schedule g_scheduler {&global_idle};
}
