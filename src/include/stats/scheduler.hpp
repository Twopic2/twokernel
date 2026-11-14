#pragma once

#include <cstdint>
#include <string_view>

namespace Stats::Scheduling {
    // Owned by a thread; callers use the scheduler's existing IRQ exclusion.
    struct ThreadStats {
        std::uint64_t ready_since_us {};
        std::uint64_t running_since_us {};
        std::uint64_t runtime_us {};
        std::uint64_t ready_wait_us {};
        std::uint64_t longest_ready_wait_us {};
        std::uint64_t dispatches {};
        bool waiting {};
        bool running {};
        bool trace_named {};
        bool trace_scopes_named {};
    };

    void ready(ThreadStats& stats, std::uint64_t now_us);
    void dispatched(ThreadStats& stats, std::uint32_t tid,
                    std::string_view name, std::uint64_t now_us);
    void stopped(ThreadStats& stats, std::uint32_t tid, std::uint64_t now_us);
}
