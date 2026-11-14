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
        // Set-1 Pause contains fake Ctrl/NumLock bytes and another E1 prefix.
        if (state_machine == State::Pause) {
            if (--pause_bytes == 0) state_machine = State::None;
            return;
        }
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
            case 0xE1:
                state_machine = State::Pause;
                pause_bytes = 5;
                return;
        }

        Arch::ScanCode curr_code;
        switch (state_machine) {
            case State::None:
                curr_code = Arch::ps2_single_byte(byte);    
                break;
            case State::E0:
                // Print Screen's E0 2A / E0 AA are fake Shift transitions.
                if ((byte & 0x7F) == 0x2A) {
                    state_machine = State::None;
                    return;
                }
                curr_code = Arch::ps2_extend_codes(byte);
                break;
            case State::Pause:
                return;
        }
        state_machine = State::None;

        const bool released = Arch::ps2_is_release(byte);
        std::uint8_t modifier = 0;
        switch (curr_code) {
            case Arch::ScanCode::LeftShift: modifier = Arch::LeftShift; break;
            case Arch::ScanCode::RightShift: modifier = Arch::RightShift; break;
            case Arch::ScanCode::LeftCtrl: modifier = Arch::LeftCtrl; break;
            case Arch::ScanCode::RightCtrl: modifier = Arch::RightCtrl; break;
            case Arch::ScanCode::LeftAlt: modifier = Arch::LeftAlt; break;
            case Arch::ScanCode::RightAlt: modifier = Arch::RightAlt; break;
            case Arch::ScanCode::CapsLock:
                if (!released && !caps_down) modifiers ^= Arch::CapsLock;
                caps_down = !released;
                break;
            case Arch::ScanCode::NumLock:
                if (!released && !num_down) modifiers ^= Arch::NumLock;
                num_down = !released;
                break;
            default: break;
        }
        if (released) modifiers &= ~modifier;
        else modifiers |= modifier;

        // Overflow must not panic in IRQ1; releases still update modifier state.
        if (!released && byte != 0 && !data.is_full()) {
            data.buf_write({curr_code, modifiers});
        }
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
