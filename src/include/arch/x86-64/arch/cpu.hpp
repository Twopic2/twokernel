#pragma once 

#include "cstdint"

namespace Cpu {
    inline constexpr std::uint32_t MSREFER = 0xC0000080;

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
}
