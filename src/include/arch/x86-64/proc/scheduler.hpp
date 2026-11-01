#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include "drivers/pit.hpp"
#include "memory/vmm.hpp"
#include <cstddef>
#include <cstdint>

using namespace Drivers::Pit;
using namespace x86::System::Gdt;
namespace x86::Proc::Scheduler {
    inline constexpr auto DEFAULT_TIME_QUANTUM = 100;
    struct Schedule {
        std::uint8_t disable_counter {}; 
        std::uint8_t task_switch_counter {};   
        bool task_switch_postponed {};
        bool time_quantum_past {};

        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::hook> waiting_queue;
        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::hook> ready_threads;
        Thread::ThreadBlock* curr_thread {};
        Thread::ThreadBlock* idle_thread {};

        void lock_scheduler();
        void unlock_scheduler(); 
        void block();
        void unblock(Thread::ThreadBlock* task);       
        void add_ready_threads(Thread::ThreadBlock* thread);
        void switch_task(Thread::ThreadBlock* next_thread);

        void yield();
        void sleep(std::uint64_t milli);

        void pit_irq_handler(ArchIrq::IrqFrame* frame);
        void schedule();
        void reschedule(); 

        // ! Use asserts for this
        bool remove_dead_process_threads() {
            auto proc = get_current_process();

            for (auto& wait_thread : waiting_queue) {
                if (wait_thread.m_process == proc) {
                    waiting_queue.remove(&wait_thread);
                }
            }

            for (auto& ready_thread: ready_threads) {
                if (ready_thread.m_process == proc) {
                    waiting_queue.remove(&ready_thread);
                }
            }    
            
            Schedule::schedule();

            return true;
        }

        inline Process::ProcessBlock* get_current_process() {
            Util::IrqRAIGaurd irq_gaurd {};

            auto thread = get_current_thread();
            return thread->m_process;
        }        

        inline Thread::ThreadBlock* get_current_thread() {
            Util::IrqRAIGaurd irq_gaurd {};
            return curr_thread; 
        }
        
        Schedule(Thread::ThreadBlock* idle) : curr_thread(idle), idle_thread(idle) {
            Util::IrqRAIGaurd irq {};
            curr_thread->state = Thread::ThreadState::Running;
            curr_thread->exec_start_ms = get_time_milliseconds();
            tss.rsp0 = reinterpret_cast<std::uint64_t>(curr_thread->rsp0);
            Cpu::bsp_cpu.syscall_kernel_rsp = tss.rsp0;
            Memory::Vmm::load_cr3(curr_thread->m_process->address_space.page_dir.pml4_pa);
        }
    };

    // TODO port this to the cpu struct 
    inline void set_idle(void*) {
        while (true) {
            asm volatile("hlt");
        }
    }

    /* 
        * Note
        * Unix has something called an Init process which is the first user process
        * the g_kernel_process is just meant for quick debugging
        * Os needs improving
    */

    inline Process::ProcessBlock g_kernel_process {"kernel", false}; 
    inline Thread::ThreadBlock g_idle  {"idle", &g_kernel_process, set_idle, nullptr};
    inline struct Schedule g_scheduler {&g_idle};
}
