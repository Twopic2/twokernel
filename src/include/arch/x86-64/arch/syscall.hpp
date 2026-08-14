#pragma once

#include "arch_irq.hpp"

namespace Syscalls {
    extern "C" void syscall_hanlder();
    extern "C" void syscall_dispatch(x86::ArchIrq::IrqFrame* frame);

    extern "C" std::uint64_t syscall_kernel_rsp;
    extern "C" std::uint64_t syscall_user_rsp;
}
