#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Stats::Trace {
    struct BufferSnapshot {
        std::size_t queued;
        std::uint64_t dropped;
    };

    // All times are PIT-derived microseconds (currently 1 ms resolution).
    std::uint64_t timestamp_us();

#ifdef TWOKERNEL_ENABLE_TRACE
    bool enabled();
    // Call once after the PIT and scheduler are initialized, with IRQs disabled.
    void start();

    // Producers copy names (up to 47 UTF-8 bytes) into a fixed-size event buffer.
    // They never allocate or write to the console; a full buffer drops events.
    void counter(std::string_view name, std::uint64_t timestamp_us,
                 std::uint64_t value);
    void interval(std::string_view name, std::uint32_t thread_id,
                  std::uint64_t start_us, std::uint64_t duration_us);
    void thread_name(std::uint32_t thread_id, std::string_view name);
    BufferSnapshot buffer_snapshot();

    // Single consumer only. Run outside IRQs and allocator/scheduler locks.
    std::size_t flush(std::size_t max_events = 128);

    namespace Detail {
        struct ScopeContext {
            std::uint32_t track_id;
            std::uint64_t start_us;
        };
        ScopeContext scope_context();
    }

    // Measures elapsed wall time, including preemption, on a separate scope track.
    class Scope {
    public:
        explicit Scope(std::string_view name);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;

    private:
        char name_[48] {};
        Detail::ScopeContext context_ {};
        bool active_ {};
    };
#else
    inline bool enabled() { return false; }
    inline void start() {}
    inline void counter(std::string_view, std::uint64_t, std::uint64_t) {}
    inline void interval(std::string_view, std::uint32_t,
                         std::uint64_t, std::uint64_t) {}
    inline void thread_name(std::uint32_t, std::string_view) {}
    inline BufferSnapshot buffer_snapshot() { return {0, 0}; }
    inline std::size_t flush(std::size_t = 128) { return 0; }
#endif
}

// Scope macros declare an object lasting until the end of the enclosing block.
// __COUNTER__ permits multiple scope markers on the same source line.
#ifdef TWOKERNEL_ENABLE_TRACE
#define TK_TRACE_DETAIL_JOIN_IMPL(a, b) a##b
#define TK_TRACE_DETAIL_JOIN(a, b) TK_TRACE_DETAIL_JOIN_IMPL(a, b)

#define TK_TRACE_COUNTER(name, value)                                             \
    do {                                                                        \
        if (::Stats::Trace::enabled()) {                                         \
            ::Stats::Trace::counter((name), ::Stats::Trace::timestamp_us(),       \
                                    (value));                                   \
        }                                                                       \
    } while (false)

#define TK_TRACE_INTERVAL(name, tid, start_us, duration_us)                       \
    do {                                                                        \
        if (::Stats::Trace::enabled()) {                                         \
            ::Stats::Trace::interval((name), (tid), (start_us), (duration_us));    \
        }                                                                       \
    } while (false)

#define TK_TRACE_SCOPE(name)                                                     \
    [[maybe_unused]] ::Stats::Trace::Scope                                       \
        TK_TRACE_DETAIL_JOIN(tk_trace_scope_, __COUNTER__) {                      \
            ::Stats::Trace::enabled() ? ::std::string_view {(name)}               \
                                      : ::std::string_view {}                    \
        }

#define TK_TRACE_FUNCTION() TK_TRACE_SCOPE(__func__)
#else
// Discard arguments entirely, including any timestamps or other side effects.
#define TK_TRACE_COUNTER(name, value) ((void)0)
#define TK_TRACE_INTERVAL(name, tid, start_us, duration_us) ((void)0)
#define TK_TRACE_SCOPE(name) ((void)0)
#define TK_TRACE_FUNCTION() ((void)0)
#endif
