#pragma once

#include <cstdint>

namespace Stats {
    struct MemorySnapshot {
        std::uint64_t managed_pages {};
        std::uint64_t allocated_pages {};
        std::uint64_t peak_allocated_pages {};
        std::uint64_t pmm_failures {};
        std::uint64_t heap_live_allocations {};
        std::uint64_t heap_requested_bytes {};
        std::uint64_t heap_backing_bytes {};
        std::uint64_t heap_peak_backing_bytes {};
        std::uint64_t heap_failures {};
    };

    // These hooks only update counters. They never allocate, log, or call PMM.
    void pmm_added(std::uint64_t pages);
    void pmm_allocated(std::uint64_t pages);
    void pmm_freed(std::uint64_t pages);
    void pmm_failed();
    void heap_allocated(std::uint64_t requested, std::uint64_t backing);
    void heap_freed(std::uint64_t requested, std::uint64_t backing);
    void heap_failed();
    MemorySnapshot memory_snapshot();
}
