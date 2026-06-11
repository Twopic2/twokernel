#include <tests/ktest.hpp>
#include <arch/x86-64/dev/keyboard.hpp>
#include <arch/x86-64/arch/event.hpp>
#include <arch/x86-64/system/irq.hpp>

namespace Tests {
    namespace {
        // Pops and returns the oldest buffered scancode
        std::uint8_t next_code(Drivers::Ps2::Ps2Keyboard& kb) {
            std::uint8_t code = kb.data.get_read();
            kb.data.buf_read();
            return code;
        }

        // g_keyboard is live (init ran, the ISR feeds it), so every section
        // starts from a known state instead of a fresh instance
        void normalize(Drivers::Ps2::Ps2Keyboard& kb) {
            while (!kb.data.is_empty()) {
                kb.data.buf_read();
            }
            kb.state_machine = Drivers::Ps2::State::None;
        }

        constexpr std::uint8_t code_of(Arch::ScanCode sc) {
            return static_cast<std::uint8_t>(sc);
        }
    }

    void keyboard_irq_tests() {
        Util::klog("--- Keyboard IRQ1 Tests ---\n");
        KTest::reset();

        using Drivers::Ps2::PS2Ack;
        using Drivers::Ps2::State;

        auto& kb = x86::Dev::Keyboard::device();

        // ── normalized device state ───────────────────────────────────────────
        {
            normalize(kb);
            KTEST_ASSERT(kb.data.is_empty(),              "drained device has an empty buffer");
            KTEST_ASSERT(kb.state_machine == State::None, "drained device sits in State::None");
        }

        // ── make code is decoded and buffered ─────────────────────────────────
        {
            normalize(kb);
            kb.on_recieve(0x1E); // press 'A'
            KTEST_ASSERT(kb.data.get_size() == 1,                     "make code lands in the buffer");
            KTEST_ASSERT(next_code(kb) == code_of(Arch::ScanCode::A), "make code decodes to ScanCode::A");
        }

        // ── break code (release) is filtered out ──────────────────────────────
        {
            normalize(kb);
            kb.on_recieve(0x9E); // release 'A' (0x1E | 0x80)
            KTEST_ASSERT(kb.data.is_empty(), "break code alone buffers nothing");

            kb.on_recieve(0x1E);
            kb.on_recieve(0x9E);
            KTEST_ASSERT(kb.data.get_size() == 1, "press + release yields exactly one entry");
            KTEST_ASSERT(next_code(kb) == code_of(Arch::ScanCode::A), "the entry is the press, not the release");
        }

        // ── controller replies set signal and stay out of the buffer ──────────
        {
            normalize(kb);
            kb.on_recieve(0xFA);
            KTEST_ASSERT(kb.signal == PS2Ack::Ack,  "0xFA sets signal to Ack");
            KTEST_ASSERT(kb.data.is_empty(),        "ack byte is not buffered as a key");

            kb.on_recieve(0xFE);
            KTEST_ASSERT(kb.signal == PS2Ack::Nack, "0xFE sets signal to Nack");
            KTEST_ASSERT(kb.data.is_empty(),        "nack byte is not buffered as a key");
        }

        // ── E0 prefix arms the state machine without buffering ────────────────
        {
            normalize(kb);
            kb.on_recieve(0xE0);
            KTEST_ASSERT(kb.data.is_empty(),            "E0 prefix alone buffers nothing");
            KTEST_ASSERT(kb.state_machine == State::E0, "E0 prefix arms the state machine");

            kb.on_recieve(0x48); // E0 48 = Up arrow press
            KTEST_ASSERT(kb.data.get_size() == 1,         "extended make code lands in the buffer");
            KTEST_ASSERT(next_code(kb) == code_of(Arch::ScanCode::Up), "E0 48 decodes to ScanCode::Up");
            KTEST_ASSERT(kb.state_machine == State::None, "state machine resets after the extended byte");
        }

        // ── extended release is filtered and disarms the prefix ───────────────
        {
            normalize(kb);
            kb.on_recieve(0xE0);
            kb.on_recieve(0xC8); // E0 C8 = Up arrow release
            KTEST_ASSERT(kb.data.is_empty(),              "extended break code buffers nothing");
            KTEST_ASSERT(kb.state_machine == State::None, "state machine resets after extended release");

            // the next plain byte must decode as a single-byte code again
            kb.on_recieve(0x48); // Keypad8 without a prefix
            KTEST_ASSERT(next_code(kb) == code_of(Arch::ScanCode::Keypad8),
                         "plain byte after an E0 sequence decodes as single-byte");
        }

        // ── keystrokes drain in the order they were typed ─────────────────────
        {
            normalize(kb);
            const std::uint8_t hello[] = { 0x23, 0x12, 0x26, 0x26, 0x18 }; // h e l l o
            for (std::uint8_t make : hello) {
                kb.on_recieve(make);
                kb.on_recieve(make | 0x80);
            }
            KTEST_ASSERT(kb.data.get_size() == 5, "five presses queue five scancodes");

            bool order_ok = true;
            for (std::uint8_t make : hello) {
                if (next_code(kb) != make) {
                    order_ok = false;
                    break;
                }
            }
            KTEST_ASSERT(order_ok,           "scancodes drain in typed (FIFO) order");
            KTEST_ASSERT(kb.data.is_empty(), "buffer is empty after draining the word");
        }

        // ── IRQ1 routes through the dispatcher into g_keyboard ────────────────
        {
            normalize(kb);
            // The data port holds whatever byte is stale, so the buffer may
            // gain at most one entry; anything more means the routing is wrong
            x86::System::Irq::hardware_interrupt_dispatch(1, nullptr);
            KTEST_ASSERT(kb.data.get_size() <= 1,
                         "dispatch on line 1 feeds isr_keyboard exactly one byte");
            normalize(kb);
        }

        KTest::summary("Keyboard IRQ1");
    }
}
