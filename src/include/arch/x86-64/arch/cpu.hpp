#pragma once 

#include "cstdint"

namespace Cpu {
    /// special CPU control registers you read/write with
    // the rdmsr/wrmsr instructions
    inline constexpr std::uint32_t MSREFER = 0xC0000080;
    inline constexpr std::uint32_t IA32_APIC_BASE = 0x1B;
    inline constexpr std::uint64_t APIC_GLOBAL_ENABLE = (1ull << 11);

    inline std::uint64_t read_msr(std::uint32_t msr) {
        std::uint32_t lo, hi;
        asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
        return (static_cast<std::uint64_t>(hi) << 32) | lo;
    }

    inline void write_msr(std::uint32_t msr, std::uint64_t value) {
        asm volatile("wrmsr" :: "c"(msr),
                     "a"(static_cast<std::uint32_t>(value)),
                     "d"(static_cast<std::uint32_t>(value >> 32)));
    }

    inline void disable_lapic() {
        write_msr(IA32_APIC_BASE, read_msr(IA32_APIC_BASE) & ~APIC_GLOBAL_ENABLE);
    }

    // Credit to Mintsuki
    inline bool cpuid(std::uint32_t leaf, std::uint32_t subleaf,
                std::uint32_t *eax, std::uint32_t *ebx, std::uint32_t *ecx, std::uint32_t *edx) {
        std::uint32_t cpuid_max;
        asm volatile ("cpuid"
                    : "=a" (cpuid_max)
                    : "a" (leaf & 0x80000000)
                    : "ebx", "ecx", "edx");
        if (leaf > cpuid_max)
            return false;
        asm volatile ("cpuid"
                    : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                    : "a" (leaf), "c" (subleaf));
        return true;
    } 

    // Disables interrupts.
    static inline void cli() {
        asm volatile("cli" ::: "memory");
    }
    /// Sets IF (Interrupt Flag) = 1 in RFLAGS which allows for interrupts
    static inline void sti() {
        asm volatile("sti" ::: "memory");
    } 
}
