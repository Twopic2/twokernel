#pragma once 

#include "pmm.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

#include "libc/string.hpp"
#include "util/kernel_logger.hpp"

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

/* 
    ! Page Directories and Tables
    ? Page Table contains the information about a single page of memory, 
    ? an entry in a page table represents the starting physical memory address for this page.

    ? Page Directory an entry in a page directory can point to depending on the page size selecte
*/
namespace Memory::Vmm {
    // Credit to @RaidTheWeb inside vmm.hpp for the idea of Direct Map
    inline constexpr std::uint64_t PRESENT = (1 << 0); // Is the page currently in physical memory? Triggers pagefault when accessed if not set -> Useful for handling swapped out memory!
    inline constexpr std::uint64_t WRITEABLE = (1 << 1); // Read-only if not set, read if set.
    inline constexpr std::uint64_t USER = (1 << 2); // Set if accessible to userspace.
    inline constexpr std::uint64_t WRITETHROUGH = (1 << 3); // Set for writethrough caching, otherwise it's writeback.
    inline constexpr std::uint64_t BIGPAGE = (1 << 7);  // Disables execution on this section of memory.
    inline constexpr std::uint64_t NX = (1ul << 63); // Requires EFER.NXE = 1 to be honored.
    inline constexpr std::uint64_t PADDR_MASK = 0x0000FFFFFFFFF000; // Getting PA from va 
    inline constexpr uint64_t OFFMASK = 0b111111111111;

    inline constexpr std::size_t PAGE_SIZE = 0x1000;

    // Direct Map way since its way easier 
    struct PageMap {
        std::array<std::uint64_t, 512> entries;
    };

    struct VAddressSpace {
        struct PageMap* pml4;
        std::uint64_t pml4_pa;
    }; 
    
    inline struct VAddressSpace kernel_space;

    // Credit to RaidtheWeb
    static inline void flush_tlb() {
        std::uint64_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        asm volatile("mov %0, %%cr3" : : "r"(cr3));
        asm volatile("lfence" : : : "memory");
    }

    static inline void load_cr3(std::uint64_t cr3_value) {
        asm volatile("mov %0, %%cr3" :: "r"(cr3_value) : "memory");
    }

    static inline std::uint64_t read_cr3() {
        std::uint64_t vaddr {};
        asm volatile("mov %%cr3, %0" : "=r"(vaddr));
        return vaddr;
    }

    inline VAddressSpace new_pagemap() {
        std::uint64_t pa = Pmm::alloc(1);
        if (pa == 0) {
            Util::klog_panic("Pmm::alloc(1) failed in new_pagemap() -- out of physical memory\n");
            return VAddressSpace {0, 0};
        }

        PageMap* pml4 = reinterpret_cast<PageMap*>(pa + Limine::hhdm.response->offset);
        LibC::memset(pml4->entries.data(), 0, sizeof(pml4->entries));

        return VAddressSpace{ .pml4 = pml4, .pml4_pa = pa };
    }

    inline void free_pagemap(std::uint64_t pa) {
        if (pa == 0) {
            Util::klog_panic("Pmm::alloc(1) failed in new_pagemap() -- out of physical memory\n");
            return;
        }
        Pmm::free(pa, 1);
    }

    PageMap* get_next_level(struct PageMap& entry, std::size_t index, std::uint64_t flags, bool allocates);

    void fill_kernel_entries(struct VAddressSpace& vaddr);
    void map_limine_kernel_addr(std::uint64_t start, std::uint64_t end, std::uint64_t flags);
    void map(struct PageMap& pagemap, std::uint64_t pa, std::uint64_t va, std::size_t length, std::uint64_t flags);
    /* PageMap no longer points to pa */
    void unmap(struct PageMap& pagemap, std::uint64_t va, std::size_t length);

    std::uint64_t va_to_pa(struct VAddressSpace& space, std::uint64_t va);
    void process_fill_kernel_entries(struct VAddressSpace& vaddr);
    
    //void* vm_alloc(std::size_t pages, std::uint64_t flags);
    //void  vm_free(void* va, std::size_t pages);

    void init();
}
