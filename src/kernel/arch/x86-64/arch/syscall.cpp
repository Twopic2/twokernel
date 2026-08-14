#include <arch/x86-64/arch/syscall.hpp>

namespace Syscalls {
    extern "C" {
        std::uint64_t syscall_kernel_rsp {};
        std::uint64_t syscall_user_rsp {};
    }

    extern "C" void syscall_dispatch(x86::ArchIrq::IrqFrame* ) {
    }
}
