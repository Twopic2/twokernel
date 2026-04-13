#pragma once

#include <cstdint>
#include <std/cstdint>

namespace x86::ArchIrq {
    struct IrqFrame {
        std::uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rsi, rdi, rdx, rcx, rbx, rax;
        std::uint64_t error;
        std::uint64_t rip;
        std::uint64_t cs;
        std::uint64_t rflags;
        std::uint64_t rsp;
        std::uint64_t ss;
    };
}