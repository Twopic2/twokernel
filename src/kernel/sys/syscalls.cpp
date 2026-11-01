#include "arch/x86-64/arch/arch_syscalls.hpp"
#include "arch/x86-64/arch/cpu.hpp"
#include "sys/syscalls.hpp"

namespace Syscalls {
    extern "C" void do_syscall_64(x86::Syscalls::SyscallFrame* frame) {
        auto syscall_num = frame->num();

        if (syscall_num >= MAX_SYSCALLS) {
            return;
        }

        *frame->ret() = global_syscall[syscall_num](frame);
    }

    // TODO: stubs until user memory mapping lands
    auto write(x86::Syscalls::SyscallFrame*) -> std::uint64_t {
        return 0;
    }

    auto brk(x86::Syscalls::SyscallFrame*) -> std::uint64_t {
        return 0;
    }
}
