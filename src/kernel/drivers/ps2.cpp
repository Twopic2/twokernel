#include "drivers/ps2.hpp"
#include "arch/x86-64/arch/event.hpp"
#include "arch/x86-64/arch/io.hpp"
#include <cstdint>

namespace Drivers::Ps2 {
    // Returns the scancode
    std::uint8_t Ps2Keyboard::read_data() {
        return x86::IO::inb(DATA_PORT);
    }

    void Ps2Keyboard::on_recieve(std::uint8_t byte) {
        switch (byte) {
            case ACK:
                signal = PS2Ack::Ack;
                nack_count = 0;
                return;
            case RESEND:
                signal = PS2Ack::Nack;
                if (++nack_count >= MAX_RETRY) {
                    nack_count = 0;
                    signal = PS2Ack::Error;
                    write_cmd(DISABLE_PORT1);
                    return;
                }
                write_data(last_data);
                return;
            case 0xE0:
                state_machine = State::E0;
                return;
        }

        Arch::ScanCode curr_code;
        switch (state_machine) {
            case State::None:
                curr_code = Arch::ps2_single_byte(byte);    
                break;
            case State::E0:
                curr_code = Arch::ps2_extend_codes(byte);
                break;
        }
        state_machine = State::None;

        if (Arch::ps2_is_release(byte)) {
            return;
        }

        data.buf_write(static_cast<std::uint8_t>(curr_code));
    }

    void Ps2Keyboard::write_cmd(std::uint8_t byte) {
        x86::IO::outb(CMD_PORT, byte);
    }
    
    void Ps2Keyboard::write_data(std::uint8_t byte) {
        last_data = byte;
        x86::IO::outb(DATA_PORT, byte);
    }

    void Ps2Keyboard::ps2_keyboard_init() {
        while (x86::IO::inb(STATUS_PORT) & 1) {                
            x86::IO::inb(DATA_PORT);
        }

        write_data(SCANNING_BYTE);
    }
}
