#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include <cstdint>

/// TODO: When I manage to set up SMP. The Cpu will handle scheduling rather than the Kernel. 
/// TODO: Maybe add a idle process list 

/// Some of linux's scheduling algo 
/// SCHED_FIFO, SCHED_RR, SCHED_DEADLINE and SCHED_OTHER

namespace x86::Proc::Scheduler {
    struct Schedule {
        std::uint8_t disable_counter {}; 
        std::uint8_t task_switch_counter {};   
        bool task_switch_postponed {};

        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::hook> sleeping_threads;
        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::hook> ready_threads;

        // Why is there an extra pointer here
        Thread::ThreadBlock* curr_ready_thread;
        Thread::ThreadBlock* curr_thread;

        void lock_scheduler();
        void unlock_scheduler(); 
        void block();
        void unblock(Thread::ThreadBlock* task);       
        Thread::ThreadBlock* get_current_thread(); 
        void add_ready_threads(Thread::ThreadBlock* thread);
        void switch_task(Thread::ThreadBlock& next_thread);
        void run_current(ArchIrq::IrqFrame* frame);
        
        void yield();
        void sleep(std::uint64_t ns);

        void pit_irq_handler(ArchIrq::IrqFrame* frame);
        void schedule();
    };

    inline struct Schedule g_scheduler {};
}
