#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include "memory/vmm.hpp"
#include "std/single_deque.hpp"
#include <arch/x86-64/proc/thread.hpp>
#include <cstdint>

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
    
    /* 
    READY: The process is runnable, and can be executed.
    RUNNING: The process is currently running.
    DEAD: The process has finished executing, and can have its resources cleaned up. 
    */

    enum class ProcStats : std::uint8_t {
        Ready,
        Running,
        Sleep,
        Dead,
    };

    using FuncPtr = void(*)(void*);
    
    struct ProcessBlock {
        kstd::SingleDeque<Thread::ThreadBlock, &Thread::ThreadBlock::proc_hook> threads;
        Thread::ThreadBlock* curr_thread {}; 
        ProcessBlock* next {};
        Memory::Vmm::VAddressSpace vaddr;
        ArchIrq::IrqFrame registers {};
        ProcStats status; 
        FuncPtr m_entry;
        const char* m_name;
        std::uint16_t pid;
        std::uint8_t thread_size {};

        ProcessBlock(const char* name, FuncPtr entry);
        ProcessBlock();

        void add_thread(Thread::ThreadBlock* thread);
        void remove_thread(Thread::ThreadBlock* thread);

        void exit();
    };
}
