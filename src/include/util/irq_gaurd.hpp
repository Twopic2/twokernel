#include "arch/x86-64/arch/cpu.hpp"
#include <cstdint>

namespace Util {
    class IrqGaurd {
        private:
            static constexpr std::uint64_t IF = (1ull << 9); 
            std::uint64_t flags;

        public:
            IrqGaurd() {
                asm volatile("pushfq; pop %0" : "=r"(flags) :: "memory");
                Cpu::cli();
            }

            ~IrqGaurd() {
                if (flags & IF) {
                    Cpu::sti();
                }
            }
    };
}
