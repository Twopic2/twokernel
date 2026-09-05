#pragma once 

#include <cstdint>

/* 
    * 64-bit SYSCALL instruction entry. Up to 6 arguments in registers.

    ? Note originally I did it where the rdi was the syscall number
    ? changes that up to rax 
*/

namespace x86::Syscalls {
    /* 
        ! Credit to Qwinci

        ? Should still be Unix style x86_64 System V ABI
    */
    struct SyscallFrame {
        std::uint64_t rax;
        std::uint64_t rbx;
        std::uint64_t rcx;
        std::uint64_t rdx;
        std::uint64_t rdi;
        std::uint64_t rsi;
        std::uint64_t rbp;
        std::uint64_t r8;
        std::uint64_t r9;
        std::uint64_t r10;
        std::uint64_t r11;
        std::uint64_t r12;
        std::uint64_t r13;
        std::uint64_t r14;
        std::uint64_t r15;

        constexpr std::uint64_t num() {
            return rax;
        }

        constexpr std::uint64_t* ret() {
            return &rax;
        }

        constexpr std::uint64_t arg0() {
            return rsi;
        }

        constexpr std::uint64_t arg1() {
            return rdi;
        }

        constexpr std::uint64_t arg2() {
            return rdx;
        }

        constexpr std::uint64_t arg3() {
            return r10;
        }

        constexpr std::uint64_t arg4() {
            return r8;
        }

        constexpr std::uint64_t arg5() {
            return r9;
        }
    };
}