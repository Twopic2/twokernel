#pragma once

#include "memory/vmm.hpp"
#include <cstdint>
#include <string_view>
#include <arch/x86-64/arch/arch_irq.hpp>
#include <std/single_deque.hpp>
#include <util/irq_gaurd.hpp>

/* 
Credit: Brendan's Multi-tasking Tutorial wiki

Thread Control Block
context switching with rsp
Kernel Stack
scheduling

While the code a thread is running can be shared, 
each thread must have its own stack and context (saved registers).
*/

/* 
Kernel thread stack size should be around 16 KiB

User space thread stacks might be higher at around 1MiB
*/

namespace x86::Proc::Process { struct ProcessBlock; }

namespace x86::Proc::Thread {
    inline constexpr std::uint64_t DEFAULT_SCHEDULE_TIME_MS = 6; 
    inline std::uint64_t total_tid {};
    inline constexpr auto SET_RFLAGS = 0x202;
    inline constexpr auto THREAD_KERNEL_SIZE = 4 * Memory::Vmm::PAGE_SIZE;

    enum class ThreadState : std::uint8_t {
        Ready,
        Running,
		Blocked,
        Sleep,
        Zombie,
        Dead
    };

    using FuncPtr = void (*)(void*);   

    /* 
        ! Note 
        ! struct member vars matters for debugger 
     */
    
    // Todo: add user stack
    struct ThreadBlock {
        std::string_view m_name {};

        std::uint64_t* rsp0; // Points highest
        std::uint64_t* kernel_stack; // Points Lowest 

        ThreadState state;
        std::uint16_t thread_id;
        ArchIrq::IrqFrame registers {};

        std::uint64_t exec_start_ms {}; // ! when the thread goes to the running queue 
        std::uint64_t total_runtime_ms {};
        std::uint64_t scheduled_time; // ! So far default is 6ms as suggested by ilobilo
        std::uint64_t sleep_time {};
        // std::uint64_t prev_runtime {}; 

        //std::uint64_t userspace_time_ms = 0;
        //std::uint64_t kernelspace_time_ms = 0;

        std::uintptr_t pa;

        FuncPtr m_entry {};

        Process::ProcessBlock* m_process {};

        kstd::SingleNode hook {};
        kstd::SingleNode proc_hook {};

        void exit();
        void reset();
        void init_kernel_stack();

        // Basically the create_kernel_task inside Brendan's Multi-taskng Tutorial
        ThreadBlock(std::string_view name, Process::ProcessBlock* process, FuncPtr entry, void* args);
        ThreadBlock();
    };  
}
