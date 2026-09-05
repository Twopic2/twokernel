#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include "memory/vmm.hpp"
#include "std/single_deque.hpp"
#include <arch/x86-64/proc/thread.hpp>
#include <cstdint>
#include <string_view>

/// NOTES:
/* 
Process: A process can be thought of as representing a single running program. 
A program can be multi-threaded, but it is still a single program, running in a 
single address space and with one set of resources. Resources in this context refer 
to things like file handles. All of this information is stored in a process control block (PCB).
*/


//* TODO: Right now there is something wrong with page faults when it comes to process. 
//* I'm going to try and get some better PF errors

namespace x86::Proc::Process {
    enum class ProcStats : std::uint8_t {
        Ready,
        Running,
		Blocked,
        Sleep,
        Zombie,
        Dead
    };

    inline std::uint16_t total_pid = 0;
    struct ProcessBlock {
        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::proc_hook> threads;
        Thread::ThreadBlock* curr_thread {}; 
        ProcessBlock* parent {};
        Memory::Vmm::VAddressSpace vaddr;
        
        ProcStats status; 
        bool is_user;
        bool is_dead;
        std::string_view m_name;
        std::uint16_t pid;
        std::uint8_t thread_size {};

        // It allocates a VMem region (0x200000 to 0x7FFFFFFFE000) for virtual memory management
        ProcessBlock(std::string_view name, bool user);
        ProcessBlock();

        void add_thread(Thread::ThreadBlock* thread);
        void remove_thread(Thread::ThreadBlock* thread);        

        void exit(int stats = 0);
        
        inline void remove_all_threads() {
            threads.clear_all();    
        }
    };
}
