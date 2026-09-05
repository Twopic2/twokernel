#include "arch/x86-64/arch/arch_syscalls.hpp"
#include "arch/x86-64/arch/cpu.hpp"
#include <arch/x86-64/arch/syscall.hpp>

namespace Syscalls {
    extern "C" void do_syscall_64(x86::Syscalls::SyscallFrame* frame) {
        auto syscall_num = frame->num();

        if (syscall_num > 60) {
            return;
        }

        *frame->ret() = global_syscall[syscall_num](frame);
    }
}
