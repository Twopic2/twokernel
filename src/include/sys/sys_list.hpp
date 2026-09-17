#pragma once 

#include "arch/x86-64/arch/arch_syscalls.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include <cstdint>

using namespace x86::Syscalls;
using namespace x86::Proc::Scheduler;
namespace Syscalls {
    inline std::uint64_t exit(x86::Syscalls::SyscallFrame* frame) {
        const auto status = static_cast<int>(frame->arg0());

        auto proc = g_scheduler.get_current_process();
        proc->remove_all_threads();
        proc->exit(status);
        
        return status;
    }

    inline std::uint64_t get_pid(x86::Syscalls::SyscallFrame*) {
        auto proc = g_scheduler.get_current_process();
        return static_cast<std::uint64_t>(proc->pid);
    }
   
    inline auto fork(x86::Syscalls::SyscallFrame*) -> std::uint64_t {
        return 0;
    } 
    
    inline auto mmap(SyscallFrame*) -> std::uint64_t {
        return 0;
    }

    auto read(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t; 
    auto write(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t; 
    auto brk(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t; 
}
