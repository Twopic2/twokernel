#include "stats/memory.hpp"
#include "util/spinlocks.hpp"

namespace Stats {
    namespace {
        // Constant initialization preserves allocations made before global ctors.
        constinit MemorySnapshot counters {};
        constinit Util::IrqSpinLock lock {};
    }

    void pmm_added(std::uint64_t pages) {
        lock.acquire();
        counters.managed_pages += pages;
        lock.release();
    }

    void pmm_allocated(std::uint64_t pages) {
        lock.acquire();
        counters.allocated_pages += pages;
        if (counters.allocated_pages > counters.peak_allocated_pages) {
            counters.peak_allocated_pages = counters.allocated_pages;
        }
        lock.release();
    }

    void pmm_freed(std::uint64_t pages) {
        lock.acquire();
        counters.allocated_pages -= pages;
        lock.release();
    }

    void pmm_failed() {
        lock.acquire();
        ++counters.pmm_failures;
        lock.release();
    }

    void heap_allocated(std::uint64_t requested, std::uint64_t backing) {
        lock.acquire();
        ++counters.heap_live_allocations;
        counters.heap_requested_bytes += requested;
        counters.heap_backing_bytes += backing;
        if (counters.heap_backing_bytes > counters.heap_peak_backing_bytes) {
            counters.heap_peak_backing_bytes = counters.heap_backing_bytes;
        }
        lock.release();
    }

    void heap_freed(std::uint64_t requested, std::uint64_t backing) {
        lock.acquire();
        --counters.heap_live_allocations;
        counters.heap_requested_bytes -= requested;
        counters.heap_backing_bytes -= backing;
        lock.release();
    }

    void heap_failed() {
        lock.acquire();
        ++counters.heap_failures;
        lock.release();
    }

    MemorySnapshot memory_snapshot() {
        lock.acquire();
        const auto snapshot = counters;
        lock.release();
        return snapshot;
    }
}
