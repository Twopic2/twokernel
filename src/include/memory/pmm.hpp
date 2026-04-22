#pragma once 

#include <cstddef>
#include <cstdint>
#include <limine/requests.hpp>

namespace Memory::Pmm {
    constexpr std::size_t page_size = 0x1000;

    struct FreeList {
        struct FreeList* next;
        std::uint64_t num_pages;
    };

    // write into or read from a physical address
    inline std::uint64_t get_hhdm() {
        return Limine::hhdm.response->offset; 
    }

    void* alloc(std::size_t count);
    void free(void* phys_mem, std::size_t size);

    void push_list(std::uint64_t phys, std::uint64_t page_amount);

    void init_pmm();
}
