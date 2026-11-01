#pragma once

#include <cstdint>
#include <cstddef>

#include "memory/heap.hpp"
#include "limine/requests.hpp"
#include "memory/pmm.hpp"
#include "memory/vmm.hpp"

/* 
    * Note
    * Very basic Kmalloc allocator meant to handle VMA new process creation etc
    * Will start on making a free list for the VMA.
*/

using namespace Memory::Vmm;

struct alignas(16) KmallocHeader {
    std::uint64_t pages;
};

[[nodiscard]] constexpr std::size_t bytes_to_pages(std::size_t bytes) {
    return bytes / PAGE_SIZE + (bytes % PAGE_SIZE != 0);
}

[[nodiscard]] std::uint8_t* kmalloc(std::size_t kb);
void kmalloc_free(std::uint8_t* k_ptr);

// ! C++ operators but for kernel memory
[[nodiscard]] void* operator new(std::size_t size);
void operator delete(void* ptr);
