#pragma once

#include "arch_irq.hpp"

/*
    ! Register convention (x86-64 System V)

    ? rax   syscall number in, return value out
    ? rdi   arg0      rsi  arg1      rdx  arg2
    ? r10   arg3      r8   arg4      r9   arg5

    ? arg3 sits in r10 rather than rcx because SYSCALL overwrites rcx with the
    ? return rip. Errors come back as a negative rax, so [-4095, -1] is an error
    ? and anything else is a result.
*/

namespace Syscalls {
    /*
        ? Not a struct of its own: isr.s and syscall.s both build an IrqFrame,
        ? and a second layout would mean a second stub. This only renames the
        ? registers to their syscall roles so handlers never index by hand.
    */
    struct SyscallFrame {
        x86::ArchIrq::IrqFrame& regs;

        constexpr std::uint64_t number() const { return regs.rax; }

        constexpr std::uint64_t arg0() const { return regs.rdi; }
        constexpr std::uint64_t arg1() const { return regs.rsi; }
        constexpr std::uint64_t arg2() const { return regs.rdx; }
        constexpr std::uint64_t arg3() const { return regs.r10; }
        constexpr std::uint64_t arg4() const { return regs.r8; }
        constexpr std::uint64_t arg5() const { return regs.r9; }

        /* The stub pops rax back out of the frame, so writing it here is the
           whole return path -- nothing else to marshal. */
        constexpr void set_return(std::uint64_t value) const { regs.rax = value; }
        constexpr void set_error(std::int64_t err) const {
            regs.rax = static_cast<std::uint64_t>(err);
        }

        constexpr bool from_user() const { return (regs.cs & 3) == 3; }
    };

    extern "C" void syscall_hanlder();
    extern "C" void syscall_dispatch(x86::ArchIrq::IrqFrame* frame);

    extern "C" std::uint64_t syscall_kernel_rsp;
    extern "C" std::uint64_t syscall_user_rsp;
}
