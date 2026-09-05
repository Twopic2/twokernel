#pragma once 

#include "arch/x86-64/arch/arch_syscalls.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include <cstdint>

namespace Syscalls {
    inline std::uint64_t exit(x86::Syscalls::SyscallFrame*) {
        return 0;
    } 

    inline std::uint64_t get_pid(x86::Syscalls::SyscallFrame*) {
        auto proc = x86::Proc::Scheduler::g_scheduler.get_current_process();
        return static_cast<std::uint64_t>(proc->pid);
    }
   
    auto fork(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t; 
    auto read(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t; 
    auto write(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t; 

}