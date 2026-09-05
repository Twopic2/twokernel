#pragma once 

#include "arch/x86-64/system/gdt.hpp"
#include "cstdint"
#include <cstddef>
#include <type_traits>

/* 
    ! SYSCALL/SYSRET
    * instructions meant for syscalls and returning them, and loads values inot the CS/SS segments 
 */
namespace Cpu {
    inline constexpr std::uint32_t MSREFER = 0xC0000080;
    // ! Ring 0 and Ring 3 Segment bases, as well as SYSCALL EIP
    inline constexpr std::uint32_t MSRSTAR = 0xc0000081;

    // ! Low 32 bits = SYSCALL EIP, bits 32-47 are kernel segment base, bits 48-63 are user segment base.
    inline constexpr std::uint32_t MSRLSTAR = 0xc0000082; // ? Kernel rip syscall 
    inline constexpr std::uint32_t MSRCSTAR = 0xc0000083; // ? The kernel's RIP for SYSCALL in compatibility mode.
    inline constexpr std::uint32_t MSRSFMASK = 0xc0000084; // ?The low 32 bits are the SYSCALL flag mask. If a bit in this is set, the corresponding bit in rFLAGS is cleared.

    // ? Credit to @RaidTheWeb
    inline const uint32_t MSRGSBASE = 0xc0000101; // ? GS
    inline const uint32_t MSRKGSBASE  = 0xc0000102; // ? Gs kernel

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

    void init_syscall();
    void init_cpu();

    struct [[gnu::packed]] OneCpu {
        std::uint64_t syscall_kernel_rsp; 
        std::uint64_t syscall_user_rsp;  
    };

    static_assert(offsetof(OneCpu, syscall_kernel_rsp) == 0);
    static_assert(offsetof(OneCpu, syscall_user_rsp) == 8);

    inline OneCpu bsp_cpu {};
}
