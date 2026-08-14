#include "arch/x86-64/arch/cpu.hpp"
#include "arch/x86-64/arch/syscall.hpp"

/* 
    ! MSR layout for AMD^$

    63          48 47          32 31                            0
    ┌──────────────┬──────────────┬───────────────────────────────┐
    │  SYSRET_CS   │  SYSCALL_CS  │   SYSCALL target EIP (legacy) │
    └──────────────┴──────────────┴───────────────────────────────┘
 */

using namespace Syscalls; 

namespace Cpu {
    // ? Credit to @RaidTheWeb
    void init_syscall() {
        write_msr(MSRKGSBASE, 0x01);

        std::uint64_t efer = read_msr(MSREFER);
        efer |= 1;
        write_msr(MSREFER, efer);

        std::uint64_t star {};
        star |= static_cast<std::uint64_t>(0x23 - 16) << 48;  
        star |= static_cast<std::uint64_t>(0x08) << 32;  
        write_msr(MSRSTAR, star);

        write_msr(MSRCSTAR, 0);
        auto entry = reinterpret_cast<std::uintptr_t>(&syscall_hanlder);
        write_msr(MSRSTAR, entry);
        write_msr(MSRSFMASK, 0x200);
    }
}
