#pragma once

#include "arch/x86-64/arch/cpu.hpp"
#include <cstdint>

namespace Util {
    inline constexpr std::uint64_t IF = (1ull << 9); 

    class IrqRAIGaurd {
        private:
            std::uint64_t flags;

        public:
            IrqRAIGaurd() {
                __asm volatile("pushfq; pop %0" : "=r"(flags) :: "memory");
                Cpu::cli();
            }

            ~IrqRAIGaurd() {
                if (flags & IF) {
                    Cpu::sti();
                }
            }
    };
}
