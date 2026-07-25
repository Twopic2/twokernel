#include <tests/ktest.hpp>
#include <std/ring_buffer.hpp>

namespace Tests {
    namespace {
        // Small instantiation for the capacity/wrap tests. One slot is kept
        // empty to tell full from empty, so usable capacity is N - 1.
        constexpr std::size_t N   = 8;
        constexpr std::size_t CAP = N - 1;
        using TestBuffer = kstd::RingBuffer<std::uint8_t, N>;
    }

    TEST_CASE("a fresh ring buffer is empty", "[ringbuffer]") {
        TestBuffer rb {};
        CHECK(rb.is_empty());
        CHECK_FALSE(rb.is_full());
        CHECK(rb.get_size() == 0);
    }

    TEST_CASE("write grows size, read shrinks it", "[ringbuffer]") {
        TestBuffer rb {};
        rb.buf_write(0x1E); // 'a' make code
        rb.buf_write(0x30); // 'b'
        rb.buf_write(0x2E); // 'c'
        CHECK(rb.get_size() == 3);
        CHECK_FALSE(rb.is_empty());
        CHECK_FALSE(rb.is_full());

        rb.buf_read();
        CHECK(rb.get_size() == 2);

        rb.buf_read();
        rb.buf_read();
        CHECK(rb.get_size() == 0);
        CHECK(rb.is_empty());
    }

    TEST_CASE("elements come out in the order they went in", "[ringbuffer]") {
        TestBuffer rb {};
        rb.buf_write(0x1E);
        rb.buf_write(0x30);
        rb.buf_write(0x2E);

        CHECK(rb.get_read() == 0x1E);
        rb.buf_read();
        CHECK(rb.get_read() == 0x30);
        rb.buf_read();
        CHECK(rb.get_read() == 0x2E);
        rb.buf_read();
    }

    TEST_CASE("interleaved reads and writes keep order", "[ringbuffer]") {
        TestBuffer rb {};
        rb.buf_write(0x10);
        rb.buf_write(0x20);

        CHECK(rb.get_read() == 0x10);
        rb.buf_read();

        rb.buf_write(0x30);
        CHECK(rb.get_size() == 2);
        CHECK(rb.get_read() == 0x20);
        rb.buf_read();
        CHECK(rb.get_read() == 0x30);
        rb.buf_read();
    }

    TEST_CASE("a full buffer drains back in FIFO order", "[ringbuffer]") {
        TestBuffer rb {};
        for (std::size_t i = 0; i < CAP; i++) {
            rb.buf_write(static_cast<std::uint8_t>(i));
        }
        CHECK(rb.is_full());
        CHECK(rb.get_size() == CAP);

        rb.buf_read();
        CHECK_FALSE(rb.is_full());

        bool order_ok = true;
        for (std::size_t i = 1; i < CAP; i++) {
            if (rb.get_read() != static_cast<std::uint8_t>(i)) {
                order_ok = false;
                break;
            }
            rb.buf_read();
        }
        CHECK(order_ok);
        CHECK(rb.is_empty());
    }

    TEST_CASE("indices wrap around the end of the array", "[ringbuffer]") {
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
        CHECK(wrap_ok);
        CHECK(rb.is_empty());
        CHECK(rb.get_size() == 0);
    }

    TEST_CASE("write wraps the array while read stays put", "[ringbuffer]") {
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
        CHECK(count == CAP);
        CHECK(rb.get_size() == CAP);
        CHECK_FALSE(rb.is_empty());
        CHECK(rb.is_full());

        bool order_ok = true;
        for (std::uint8_t i = 0; i < count; i++) {
            if (rb.get_read() != i) {
                order_ok = false;
                break;
            }
            rb.buf_read();
        }
        CHECK(order_ok);
        CHECK(rb.is_empty());
    }

    TEST_CASE("elements wider than a byte survive without truncation", "[ringbuffer]") {
        kstd::RingBuffer<std::uint16_t, 4> rb {};
        rb.buf_write(0xE048); // extended scancode, doesn't fit in uint8_t
        rb.buf_write(0x1234);
        CHECK(rb.get_read() == 0xE048);
        rb.buf_read();
        CHECK(rb.get_read() == 0x1234);
        rb.buf_read();
        CHECK(rb.is_empty());
    }

    TEST_CASE("the KeyboardBuffer alias is a working instantiation", "[ringbuffer]") {
        kstd::KeyboardBuffer kb {};
        CHECK(kb.is_empty());
        kb.buf_write(0x1E);
        CHECK(kb.get_read() == 0x1E);
        kb.buf_read();
        CHECK(kb.is_empty());
    }
}
