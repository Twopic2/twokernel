#pragma once

#include "arch/x86-64/arch/arch_syscalls.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "arch/x86-64/arch/sys_list.hpp"
#include "arch_irq.hpp"
#include <array>
#include <cstdint>
#include <expected>

/* 
    ! Syscall Notes
    When userspace executes syscall:

    CPU saves: rip → rcx, rflags → r11, then clears IF
    CPU loads: rip from IA32_LSTAR MSR (set to entry_SYSCALL_64 at boot)
    CPU switches: CS/SS from IA32_STAR MSR (kernel segments)

    Rax would be the return register


    This would be different from how IRETQ does things 
*/

namespace Syscalls {
    extern "C" void syscall_hanlder();
    extern "C" void do_syscall_64(x86::Syscalls::SyscallFrame* frame);

    constexpr auto MAX_SYSCALLS = 512;

    using syscall_fn = auto (*)(x86::Syscalls::SyscallFrame* frame) -> std::uint64_t;
    
    /** 
        @note Here's the https://www.chromium.org/chromium-os/developer-library/reference/linux-constants/syscalls/
    */
    inline std::array<syscall_fn, MAX_SYSCALLS> global_syscall {};

    inline void init_syscalls() {
        global_syscall[39] = get_pid;
        global_syscall[60] = exit;
    }
}
