#include "stats/scheduler.hpp"
#include "stats/trace.hpp"

namespace Stats::Scheduling {
    void ready(ThreadStats& stats, std::uint64_t now_us) {
        if (!stats.waiting) {
            stats.ready_since_us = now_us;
            stats.waiting = true;
        }
    }

    void dispatched(ThreadStats& stats, std::uint32_t tid,
                    std::string_view name, std::uint64_t now_us) {
        if (Trace::enabled() && !stats.trace_named) {
            Trace::thread_name(tid, name);
            stats.trace_named = true;
        }
        if (stats.waiting && now_us >= stats.ready_since_us) {
            const auto wait = now_us - stats.ready_since_us;
            stats.ready_wait_us += wait;
            if (wait > stats.longest_ready_wait_us) {
                stats.longest_ready_wait_us = wait;
            }
            Trace::interval("Ready", tid, stats.ready_since_us, wait);
        }
        stats.waiting = false;
        stats.running_since_us = now_us;
        stats.running = true;
        ++stats.dispatches;
    }

    void stopped(ThreadStats& stats, std::uint32_t tid, std::uint64_t now_us) {
        if (stats.running && now_us >= stats.running_since_us) {
            const auto runtime = now_us - stats.running_since_us;
            stats.runtime_us += runtime;
            Trace::interval("Running", tid, stats.running_since_us, runtime);
        }
        stats.running = false;
    }
}
