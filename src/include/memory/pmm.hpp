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
    constexpr std::size_t page_size = 0x1000;

    struct FreeList {
        struct FreeList* next;
        std::uint64_t num_pages;
        std::uint64_t pa;
    };

    std::uintptr_t alloc(std::size_t count);
    void free(void* phys_mem, std::size_t size);

    void push_list(std::uint64_t phys, std::uint64_t page_amount);

    void init_pmm();
}
