#pragma once 

#include <cstddef>
#include <cstdint>
#include <limine/requests.hpp>

/* 
Taking from osdev notes;
Commonly we typically claim the entire higher half of the virtual address space for use by the kernel, 
and leave the entire lower half alone for userspace.
 */

namespace Memory::Pmm {
    struct FreeList {
        struct FreeList* next;
        std::uint64_t num_pages;
        std::uint64_t pa;
    };

    static FreeList* freelist_head {};

    std::uint64_t alloc(std::uint64_t count);
    void free(std::uint64_t phys_mem, std::uint64_t size);
    void reclaim_bootloader(std::uint64_t memap_type, std::uint64_t phys_mem, std::uint64_t page_amount);

    void push_list(std::uint64_t phys, std::uint64_t page_amount);

    void init_pmm();
}
