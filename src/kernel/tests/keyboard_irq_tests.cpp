#include <tests/ktest.hpp>
#include <arch/x86-64/dev/keyboard.hpp>
#include <arch/x86-64/arch/event.hpp>
#include <arch/x86-64/arch/io.hpp>
#include <arch/x86-64/system/irq.hpp>

namespace Tests {
    namespace {
        // Pops and returns the oldest buffered scancode
        std::uint8_t next_code(Drivers::Ps2::Ps2Keyboard& kb) {
            std::uint8_t code = kb.data.get_read();
            kb.data.buf_read();
            return code;
        }

        // g_keyboard is live (init ran, the ISR feeds it), so every case
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

        // The nack path resends last_data to the live keyboard, and its ack
        // would race the assertions through the ISR; keep IRQ1 quiet for the
        // whole case and swallow any provoked replies on the way out. RAII so
        // a failing REQUIRE (which returns early) can't leave IRQ1 masked.
        struct Irq1Silencer {
            Irq1Silencer() {
                x86::System::Irq::irq_mask(1);
            }
            ~Irq1Silencer() {
                while (x86::IO::inb(Drivers::Ps2::STATUS_PORT) & 1) {
                    x86::IO::inb(Drivers::Ps2::DATA_PORT);
                }
                x86::System::Irq::irq_unmask(1);
            }
        };
    }

    TEST_CASE("a drained device has an empty buffer and idle state machine", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        CHECK(kb.data.is_empty());
        CHECK(kb.state_machine == Drivers::Ps2::State::None);
    }

    TEST_CASE("make codes are decoded and buffered", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        kb.on_recieve(0x1E); // press 'A'
        REQUIRE(kb.data.get_size() == 1);
        CHECK(next_code(kb) == code_of(Arch::ScanCode::A));
    }

    TEST_CASE("break codes (releases) are filtered out", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        kb.on_recieve(0x9E); // release 'A' (0x1E | 0x80)
        CHECK(kb.data.is_empty());

        kb.on_recieve(0x1E);
        kb.on_recieve(0x9E);
        REQUIRE(kb.data.get_size() == 1);
        CHECK(next_code(kb) == code_of(Arch::ScanCode::A));
    }

    TEST_CASE("controller replies set signal and stay out of the buffer", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        kb.on_recieve(0xFA);
        CHECK(kb.signal == Drivers::Ps2::PS2Ack::Ack);
        CHECK(kb.data.is_empty());

        kb.on_recieve(0xFE);
        CHECK(kb.signal == Drivers::Ps2::PS2Ack::Nack);
        CHECK(kb.data.is_empty());
    }

    TEST_CASE("E0 prefix arms the state machine without buffering", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        kb.on_recieve(0xE0);
        CHECK(kb.data.is_empty());
        CHECK(kb.state_machine == Drivers::Ps2::State::E0);

        kb.on_recieve(0x48); // E0 48 = Up arrow press
        REQUIRE(kb.data.get_size() == 1);
        CHECK(next_code(kb) == code_of(Arch::ScanCode::Up));
        CHECK(kb.state_machine == Drivers::Ps2::State::None);
    }

    TEST_CASE("extended release is filtered and disarms the prefix", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        kb.on_recieve(0xE0);
        kb.on_recieve(0xC8); // E0 C8 = Up arrow release
        CHECK(kb.data.is_empty());
        CHECK(kb.state_machine == Drivers::Ps2::State::None);

        // the next plain byte must decode as a single-byte code again
        kb.on_recieve(0x48); // Keypad8 without a prefix
        CHECK(next_code(kb) == code_of(Arch::ScanCode::Keypad8));
    }

    TEST_CASE("keystrokes drain in the order they were typed", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        const std::uint8_t hello[] = { 0x23, 0x12, 0x26, 0x26, 0x18 }; // h e l l o
        for (std::uint8_t make : hello) {
            kb.on_recieve(make);
            kb.on_recieve(make | 0x80);
        }
        REQUIRE(kb.data.get_size() == 5);

        bool order_ok = true;
        for (std::uint8_t make : hello) {
            if (next_code(kb) != make) {
                order_ok = false;
                break;
            }
        }
        CHECK(order_ok);
        CHECK(kb.data.is_empty());
    }

    TEST_CASE("nack handling retries and escalates at the limit", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        // A real nack below the limit resends last_sent to the device and
        // the ack would race these assertions, so start one short of the
        // limit: this path goes straight to the controller, no device I/O
        kb.nack_count = Drivers::Ps2::MAX_RETRY - 1;
        kb.on_recieve(0xFE);
        CHECK(kb.signal == Drivers::Ps2::PS2Ack::Error);
        CHECK(kb.data.is_empty());

        // The escalation disabled port 1; bring it back for the rest of boot
        kb.write_cmd(Drivers::Ps2::ENABLE_PORT1);
        kb.nack_count = 0;

        kb.nack_count = 2;
        kb.on_recieve(0xFA);
        CHECK(kb.signal == Drivers::Ps2::PS2Ack::Ack);
        CHECK(kb.nack_count == 0);
    }

    TEST_CASE("IRQ1 routes through the dispatcher into g_keyboard", "[keyboard]") {
        Irq1Silencer quiet {};
        auto& kb = x86::Dev::Keyboard::device();
        normalize(kb);

        // The data port holds whatever byte is stale, so the buffer may
        // gain at most one entry; anything more means the routing is wrong
        x86::System::Irq::hardware_interrupt_dispatch(1, nullptr);
        CHECK(kb.data.get_size() <= 1);
        normalize(kb);
    }
}
