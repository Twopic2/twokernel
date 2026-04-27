#pragma once

// So the most optimal way is to use a RB-tree which allows for O(log n) searching. 
// But I'm just using a sorted Linked List of Virtual address space for the Kernel/Process
// VmSpace allows to use heap memory

#include <cstdint>
namespace Memory::Virt {
    // Credit to RaidTheWeb
    enum class flags : std::uint8_t {
        VIRT_RW         = (1 << 0), // Region is writeable.
        VIRT_USER       = (1 << 1), // Region is unprivileged.
        VIRT_NX         = (1 << 2), // Region is non-executable.
        VIRT_SWAPPED    = (1 << 3), // Region is swapped out to disk.
        VIRT_DIRTY      = (1 << 4), // Region has been modified (and is thus, dirty). Automatic.
        VIRT_SHARED     = (1 << 5), // Region is shared between processses (no CoW!).
        VIRT_CHRSPECIAL = (1 << 6)  // Region is backed by a character special file. No demand paging.
    };

    struct VmSpace {
        VmSpace* next;
        std::uintptr_t base;
        std::uintptr_t length;
        std::uint8_t flags;
    };

    std::uintptr_t vmspace_alloc(struct VmSpace& space, std::size_t size, std::uint8_t flags);
    void vmspace_free(std::uintptr_t obj);
}