#pragma once 

#include "pmm.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

#include "libc/string.hpp"


/// NOTES:
/*

Level 4 paging 
PML4 -> PDP -> PD -> PT

Virtual Address → PML4 → PDPT → PD → PT → Physical Address

| 9 bits | 9 bits | 9 bits | 9 bits | 12 bits |
| PML4   | PDPT   | PD     | PT     | Offset  |

2^9 = 512 entries
2^12 = 4096 page offset 

The number of levels depend on the size of the pages chosen. 
If we are using 4Kib pages then we will have: PML4, PDPR, PD, PT, while 
if we go for 2Mib Pages we have only PML4, PDPR, PD, and finally 1Gib pages would only use the PML4 and PDPR.
*/

namespace Memory::Vmm {
    // Credit to @RaidTheWeb inside vmm.hpp for the idea of Direct Map
    inline constexpr uint64_t PRESENT = (1 << 0); // Is the page currently in physical memory? Triggers pagefault when accessed if not set -> Useful for handling swapped out memory!
    inline constexpr uint64_t WRITEABLE = (1 << 1); // Read-only if not set, read if set.
    inline constexpr uint64_t USER = (1 << 2); // Set if accessible to userspace.
    inline constexpr uint64_t BIGPAGE = (1 << 7); // Sets Page Dir which would be 2mb page 

    // Direct Map way since its way easier 
    struct PageMap {
        std::array<std::uint64_t, 512> entries;
    };

    struct AddrSpace {
        struct PageMap* pml4;
        std::uintptr_t pml4_pa;
    }; 
    
    inline struct AddrSpace kernel_space;

    static inline void load_cr3(uint64_t cr3_value) {
        asm volatile("mov %0, %%cr3" :: "r"(cr3_value) : "memory");
    }

    static inline AddrSpace new_pagemap() {
        std::uintptr_t pa = Pmm::alloc(1);

        PageMap* pml4 = reinterpret_cast<PageMap*>(pa + Limine::hhdm.response->offset);
        pml4->entries.fill(0);

        return AddrSpace{ .pml4 = pml4, .pml4_pa = pa };
    }

    static inline std::uint64_t convert_flag(std::uint64_t flags) {
        std::uint64_t value = PRESENT;

        if (flags & WRITEABLE)  value |= WRITEABLE;
        if (flags & USER)       value |= USER;
        if (flags & BIGPAGE)    value |= BIGPAGE;

        return value;
    }

    auto get_level(struct PageMap& entry);

    auto get_pte(std::uintptr_t va);

    void alloc(std::uint64_t size);
    void free(void* addr);
    
    void map(std::uintptr_t pa, std::uintptr_t va, std::size_t length, std::uint64_t flags);
    void unmap(std::uintptr_t pa, std::uintptr_t va, std::size_t length);

    void init_vm();
}
