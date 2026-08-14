#include "arch/x86-64/arch/cpu.hpp"
#include "arch/x86-64/arch/syscall.hpp"
#include "util/kernel_logger.hpp"
#include "util/ansi.hpp"

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

        Util::klog("syscall: STAR=0x%llx (sysret base 0x%llx, syscall cs 0x%llx)\n",
                   star, star >> 48, (star >> 32) & 0xFFFF);

        write_msr(MSRCSTAR, 0);

        auto entry = reinterpret_cast<std::uintptr_t>(&syscall_hanlder);
        write_msr(MSRLSTAR, entry);
        write_msr(MSRSFMASK, 0x200);

        Util::klog("syscall: LSTAR=0x%llx SFMASK=0x%llx EFER.SCE=%u\n",
                   entry, read_msr(MSRSFMASK),
                   static_cast<unsigned>(read_msr(MSREFER) & 1));

      
        if (read_msr(MSRLSTAR) != entry) {
            Util::klog_panic("syscall: LSTAR readback mismatch\n");
        }

        Util::klog("%s%ssyscall: entry configured%s\n", BOLD, GREEN, RESET);
    }
}
