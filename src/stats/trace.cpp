#include "stats/trace.hpp"
#include "drivers/pit.hpp"

namespace Stats::Trace {
    std::uint64_t timestamp_us() {
        return Drivers::Pit::get_time_milliseconds() * 1000;
    }
}

#ifdef TWOKERNEL_ENABLE_TRACE
#include "arch/x86-64/arch/io.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "memory/heap.hpp"
#include "stats/memory.hpp"
#include "util/irq_gaurd.hpp"
#include "util/spinlocks.hpp"

namespace Stats::Trace {
    namespace {
        constexpr std::size_t capacity = 1024;
        enum class Kind { Counter, Interval, ThreadName };
        struct Event {
            Kind kind;
            std::uint32_t tid;
            std::uint64_t timestamp;
            std::uint64_t value;
            char name[48] {};
        };

        constinit Event events[capacity] {};
        constinit Util::IrqSpinLock queue_lock {};
        constinit std::atomic<bool> active {false};
        std::size_t head {}, count {};
        std::uint64_t dropped {};

        void enqueue(Kind kind, std::uint32_t tid, std::string_view name,
                     std::uint64_t timestamp, std::uint64_t value) {
            if (!active.load(std::memory_order_relaxed)) {
                return;
            }
            Event event {kind, tid, timestamp, value, {}};
            auto length = name.size() < sizeof(event.name)
                ? name.size() : sizeof(event.name) - 1;
            // Avoid splitting a UTF-8 codepoint when truncating a name.
            if (length < name.size()) {
                while (length && (static_cast<unsigned char>(name[length]) & 0xc0) == 0x80) {
                    --length;
                }
            }
            for (std::size_t i = 0; i < length; ++i) {
                event.name[i] = name[i];
            }

            queue_lock.acquire();
            if (count == capacity) {
                ++dropped;
            } else {
                events[(head + count) % capacity] = event;
                ++count;
            }
            queue_lock.release();
        }

        bool dequeue(Event& event) {
            queue_lock.acquire();
            if (count == 0) {
                queue_lock.release();
                return false;
            }
            event = events[head];
            head = (head + 1) % capacity;
            --count;
            queue_lock.release();
            return true;
        }

        void write(const char* text) {
            while (*text) {
                x86::IO::outb(0xe9, static_cast<std::uint8_t>(*text++));
            }
        }

        void number(std::uint64_t value) {
            char digits[20];
            std::size_t length = 0;
            do {
                digits[length++] = '0' + value % 10;
                value /= 10;
            } while (value);
            while (length) {
                x86::IO::outb(0xe9, digits[--length]);
            }
        }

        void quoted(const char* text) {
            constexpr char hex[] = "0123456789abcdef";
            write("\"");
            while (*text) {
                const auto ch = static_cast<unsigned char>(*text++);
                if (ch == '"' || ch == '\\') {
                    write("\\");
                } else if (ch < 0x20) {
                    write("\\u00");
                    x86::IO::outb(0xe9, hex[ch >> 4]);
                    x86::IO::outb(0xe9, hex[ch & 15]);
                    continue;
                }
                x86::IO::outb(0xe9, ch);
            }
            write("\"");
        }

        void export_event(const Event& event) {
            write("{\"pid\":1,\"tid\":");
            number(event.tid);
            write(",\"ts\":");
            number(event.timestamp);
            if (event.kind == Kind::ThreadName) {
                write(",\"ph\":\"M\",\"name\":\"thread_name\",\"args\":{\"name\":");
                quoted(event.name);
            } else {
                write(event.kind == Kind::Counter
                    ? ",\"ph\":\"C\",\"name\":" : ",\"ph\":\"X\",\"name\":");
                quoted(event.name);
                if (event.kind == Kind::Interval) {
                    write(",\"dur\":");
                    number(event.value);
                    write("}\n");
                    return;
                }
                write(",\"args\":{\"value\":");
                number(event.value);
            }
            write("}}\n");
        }

        void sample_memory(std::uint64_t now) {
            const auto memory = memory_snapshot();
            constexpr auto page = Memory::Vmm::PAGE_SIZE;
            counter("PMM managed bytes", now, memory.managed_pages * page);
            counter("PMM allocated bytes", now, memory.allocated_pages * page);
            counter("PMM peak allocated bytes", now, memory.peak_allocated_pages * page);
            counter("PMM allocation failures", now, memory.pmm_failures);
            counter("Heap requested bytes", now, memory.heap_requested_bytes);
            counter("Heap backing bytes", now, memory.heap_backing_bytes);
            counter("Heap peak backing bytes", now, memory.heap_peak_backing_bytes);
            counter("Heap live allocations", now, memory.heap_live_allocations);
            counter("Heap allocation failures", now, memory.heap_failures);
            counter("Trace dropped events", now, buffer_snapshot().dropped);
        }

        [[noreturn]] void worker(void*) {
            write("{\"ph\":\"M\",\"pid\":1,\"name\":\"process_name\","
                  "\"args\":{\"name\":\"TwoKernel\"}}\n");
            std::uint64_t next_sample = 0;
            for (;;) {
                const auto now = timestamp_us();
                if (now >= next_sample) {
                    sample_memory(now);
                    next_sample = now + 100'000;
                }
                flush();
                // Stay runnable: this scheduler resumes contexts at IRQ return.
                asm volatile("sti; hlt" ::: "memory");
            }
        }
    }

    bool enabled() {
        return active.load(std::memory_order_relaxed);
    }

    Detail::ScopeContext Detail::scope_context() {
        Util::IrqRAIGaurd irq {};
        auto* thread = x86::Proc::Scheduler::g_scheduler.curr_thread;
        // Kernel TIDs are uint16_t. Keep instrumentation off scheduler tracks:
        // a scope may span several non-nested Running/Ready intervals.
        const std::uint32_t track_id = 0x10000u + (thread ? thread->thread_id : 0u);
        if (thread && !thread->profiling.trace_scopes_named) {
            constexpr std::string_view prefix = "Scopes: ";
            char label[48] {};
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                label[i] = prefix[i];
            }
            auto length = thread->m_name.size();
            const auto available = sizeof(label) - prefix.size() - 1;
            if (length > available) {
                length = available;
                while (length && (static_cast<unsigned char>(thread->m_name[length]) & 0xc0) == 0x80) {
                    --length;
                }
            }
            for (std::size_t i = 0; i < length; ++i) {
                label[prefix.size() + i] = thread->m_name[i];
            }
            thread_name(track_id, label);
            thread->profiling.trace_scopes_named = true;
        }
        return {track_id, timestamp_us()};
    }

    void start() {
        if (active.exchange(true, std::memory_order_relaxed)) {
            return;
        }
        using namespace x86::Proc;
        auto* current = Scheduler::g_scheduler.curr_thread;
        Scheduling::dispatched(current->profiling, current->thread_id,
                               current->m_name, timestamp_us());
        auto* thread = new Thread::ThreadBlock {
            "trace exporter", &Scheduler::g_kernel_process, worker, nullptr};
        Scheduler::g_kernel_process.add_thread(thread);
        Scheduler::g_scheduler.add_ready_threads(thread);
    }

    void counter(std::string_view name, std::uint64_t timestamp,
                 std::uint64_t value) {
        enqueue(Kind::Counter, 0, name, timestamp, value);
    }

    void interval(std::string_view name, std::uint32_t tid,
                  std::uint64_t start, std::uint64_t duration) {
        if (duration) {
            enqueue(Kind::Interval, tid, name, start, duration);
        }
    }

    void thread_name(std::uint32_t tid, std::string_view name) {
        enqueue(Kind::ThreadName, tid, name, timestamp_us(), 0);
    }

    BufferSnapshot buffer_snapshot() {
        queue_lock.acquire();
        const BufferSnapshot snapshot {count, dropped};
        queue_lock.release();
        return snapshot;
    }

    std::size_t flush(std::size_t max_events) {
        Event event {};
        std::size_t written = 0;
        while (written < max_events && dequeue(event)) {
            export_event(event);
            ++written;
        }
        return written;
    }
}
#endif
