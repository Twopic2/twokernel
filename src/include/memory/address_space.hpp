#pragma once

#include "single_deque.hpp"
#include "vmm.hpp"
#include <cstdint>
#include <new>
#include <string_view>

/* 
    * address spcae 

    *Close to how SerientyOs does things with AddressSpace.h handles
    * Stores the pagedirectory

    *Currently all the regions are managed through a deque but should change it to a tree

      * address space:  [--obj A--]      [----obj B----]          [-obj C-]
    *                            ^^^^^^^^             ^^^^^^^^^^
    *                             free gap             free gap   (implicit — no nodes)
    

    * This is meant to be a region list including User Heap, User Stack, Code, and Data
    
    vmem → [Code] → [Data] → [User Heap] → [User Stack] → nullptr

    * If we need user stack memory with let's say call stack, we begin at 0x7FFFFFFFF000 and allocate downwards
    * User stack should be around 1mb

*/
using namespace Memory::Vmm;

inline constexpr std::uint64_t USER_STACK_TOP = 0x7FFFFFFFF000;
inline constexpr std::uint64_t USER_STACK_SIZE = 0x100000;
inline constexpr std::uint64_t USER_STACK_BOTTOM = USER_STACK_TOP - USER_STACK_SIZE;

inline constexpr std::uint64_t USER_ALLOC_BASE = 0x4000; 
inline constexpr std::uint64_t USER_HEAP_END = USER_STACK_TOP - USER_STACK_SIZE - Memory::Vmm::PAGE_SIZE;

namespace Memory::AddrSpace {
    enum class VmemLocation : std::uint8_t {
        HEAP,
        STACK,
        CODE,
        DATA        
    };

    inline VmemLocation calculate_vmem_space(const VmemLocation& vm_loc);

    struct VmRegion {
        std::uint64_t m_base;
        std::uint64_t m_length;
        std::uint64_t m_flags;
        VmemLocation m_location;

        VmRegion(std::uint64_t base, std::uint64_t length, std::uint64_t flags) : 
        m_base(base), m_length(length), m_flags(flags) {
        }

        auto operator<=>(const VmRegion& other) const;

        kstd::SingleNode hook; 
    };

    struct AddressSpace {
            PageDirectory page_dir;
            kstd::SingleDeque<VmRegion, &VmRegion::hook> vmem;

            AddressSpace() {
                page_dir = new_pagemap();
            }

            AddressSpace(const AddressSpace&) = delete;
            AddressSpace& operator=(const AddressSpace&) = delete;

            void allocate_user_heap(std::uint64_t size_kb);
            std::uint64_t calculate_base_and_bound();
    }; 
}
