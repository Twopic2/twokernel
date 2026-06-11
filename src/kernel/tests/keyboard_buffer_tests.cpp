#include <tests/ktest.hpp>
#include <std/ring_buffer.hpp>

namespace Tests {
    void keyboard_buffer_tests() {
        Util::klog("--- RingBuffer Tests ---\n");
        KTest::reset();

        // Small instantiation for the capacity/wrap tests. One slot is kept
        // empty to tell full from empty, so usable capacity is N - 1.
        constexpr std::size_t N   = 8;
        constexpr std::size_t CAP = N - 1;
        using TestBuffer = kstd::RingBuffer<std::uint8_t, N>;

        // ── fresh buffer state ────────────────────────────────────────────────
        {
            TestBuffer rb {};
            KTEST_ASSERT(rb.is_empty(),      "fresh buffer is empty");
            KTEST_ASSERT(!rb.is_full(),      "fresh buffer is not full");
            KTEST_ASSERT(rb.get_size() == 0, "fresh buffer has size 0");
        }

        // ── write grows size, read shrinks it ─────────────────────────────────
        {
            TestBuffer rb {};
            rb.buf_write(0x1E); // 'a' make code
            rb.buf_write(0x30); // 'b'
            rb.buf_write(0x2E); // 'c'
            KTEST_ASSERT(rb.get_size() == 3, "size is 3 after three writes");
            KTEST_ASSERT(!rb.is_empty(),     "buffer is not empty after writes");
            KTEST_ASSERT(!rb.is_full(),      "buffer is not full after three writes");

            rb.buf_read();
            KTEST_ASSERT(rb.get_size() == 2, "read shrinks size");

            rb.buf_read();
            rb.buf_read();
            KTEST_ASSERT(rb.get_size() == 0, "size returns to 0 after draining");
            KTEST_ASSERT(rb.is_empty(),      "buffer is empty after draining");
        }

        // ── FIFO order: elements come out in the order they went in ───────────
        {
            TestBuffer rb {};
            rb.buf_write(0x1E);
            rb.buf_write(0x30);
            rb.buf_write(0x2E);

            KTEST_ASSERT(rb.get_read() == 0x1E, "first element out is the first one in");
            rb.buf_read();
            KTEST_ASSERT(rb.get_read() == 0x30, "second element follows the first");
            rb.buf_read();
            KTEST_ASSERT(rb.get_read() == 0x2E, "third element follows the second");
            rb.buf_read();
        }

        // ── interleaved reads and writes keep order ───────────────────────────
        {
            TestBuffer rb {};
            rb.buf_write(0x10);
            rb.buf_write(0x20);

            KTEST_ASSERT(rb.get_read() == 0x10, "head is the oldest unread element");
            rb.buf_read();

            rb.buf_write(0x30);
            KTEST_ASSERT(rb.get_size() == 2,    "size reflects one read and one new write");
            KTEST_ASSERT(rb.get_read() == 0x20, "older element still comes out before the new one");
            rb.buf_read();
            KTEST_ASSERT(rb.get_read() == 0x30, "newest element comes out last");
            rb.buf_read();
        }

        // ── fill to capacity, then drain everything back in order ─────────────
        {
            TestBuffer rb {};
            for (std::size_t i = 0; i < CAP; i++) {
                rb.buf_write(static_cast<std::uint8_t>(i));
            }
            KTEST_ASSERT(rb.is_full(),          "buffer is full at capacity");
            KTEST_ASSERT(rb.get_size() == CAP,  "size equals capacity when full");

            rb.buf_read();
            KTEST_ASSERT(!rb.is_full(), "one read makes a full buffer not-full again");

            bool order_ok = true;
            for (std::size_t i = 1; i < CAP; i++) {
                if (rb.get_read() != static_cast<std::uint8_t>(i)) {
                    order_ok = false;
                    break;
                }
                rb.buf_read();
            }
            KTEST_ASSERT(order_ok,      "a full buffer drains back in FIFO order");
            KTEST_ASSERT(rb.is_empty(), "buffer is empty after a full drain");
        }

        // ── indices wrap around the end of the array correctly ────────────────
        {
            TestBuffer rb {};
            // Push one element through at a time for several laps of the
            // array, so read/write both wrap past index N-1 back to 0.
            bool wrap_ok = true;
            for (std::size_t i = 0; i < 3 * N; i++) {
                rb.buf_write(static_cast<std::uint8_t>(i));
                if (rb.get_read() != static_cast<std::uint8_t>(i)) {
                    wrap_ok = false;
                    break;
                }
                rb.buf_read();
            }
            KTEST_ASSERT(wrap_ok,            "elements survive the indices wrapping around");
            KTEST_ASSERT(rb.is_empty(),      "buffer is empty after the wrap laps");
            KTEST_ASSERT(rb.get_size() == 0, "size is 0 after the wrap laps");
        }

        // ── write index wraps the array while read stays put ──────────────────
        {
            TestBuffer rb {};
            // Push read off index 0 so write has somewhere to wrap into.
            for (std::uint8_t i = 0; i < 3; i++) {
                rb.buf_write(i);
            }
            for (std::uint8_t i = 0; i < 3; i++) {
                rb.buf_read();
            }

            // read sits at 3. Filling until full walks write through
            // 3..N-1, around the end of the array, and back up to 2 —
            // one slot behind read, never on top of it.
            std::uint8_t count = 0;
            while (!rb.is_full()) {
                rb.buf_write(count);
                count++;
            }
            KTEST_ASSERT(count == CAP,          "write wraps and stops one slot behind read");
            KTEST_ASSERT(rb.get_size() == CAP,  "size is not zeroed when write wraps but read hasn't");
            KTEST_ASSERT(!rb.is_empty(),        "wrapped-full buffer does not report empty");
            KTEST_ASSERT(rb.is_full(),          "wrapped-full buffer reports full");

            bool order_ok = true;
            for (std::uint8_t i = 0; i < count; i++) {
                if (rb.get_read() != i) {
                    order_ok = false;
                    break;
                }
                rb.buf_read();
            }
            KTEST_ASSERT(order_ok,      "FIFO order survives write wrapping past the array end");
            KTEST_ASSERT(rb.is_empty(), "buffer is empty after draining the wrapped data");
        }

        // ── elements wider than a byte are stored without truncation ──────────
        {
            kstd::RingBuffer<std::uint16_t, 4> rb {};
            rb.buf_write(0xE048); // extended scancode, doesn't fit in uint8_t
            rb.buf_write(0x1234);
            KTEST_ASSERT(rb.get_read() == 0xE048, "16-bit element survives intact");
            rb.buf_read();
            KTEST_ASSERT(rb.get_read() == 0x1234, "second 16-bit element survives intact");
            rb.buf_read();
            KTEST_ASSERT(rb.is_empty(),           "wide-element buffer drains to empty");
        }

        // ── the KeyboardBuffer alias is a working instantiation ───────────────
        {
            kstd::KeyboardBuffer kb {};
            KTEST_ASSERT(kb.is_empty(), "KeyboardBuffer starts empty");
            kb.buf_write(0x1E);
            KTEST_ASSERT(kb.get_read() == 0x1E, "KeyboardBuffer round-trips a scancode");
            kb.buf_read();
            KTEST_ASSERT(kb.is_empty(), "KeyboardBuffer drains to empty");
        }

        KTest::summary("RingBuffer");
    }
}
