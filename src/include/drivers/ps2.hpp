#pragma once

#include "arch/x86-64/arch/event.hpp"
#include "std/ring_buffer.hpp"
#include <cstdint>

/* 
So there is a controller (8042) which similar to the PIC the cpu never directly talks to it. 
This is why we have drivers which wires port 1/2 (0x60/0x64)

Keyboard uses port 1
*/

namespace Drivers::Ps2 {
    enum class PS2Ack {
        Ack,
        Nack,
        Error
    };

    enum class State : std::uint8_t {
        None,
        E0,
    };

    inline constexpr std::uint8_t DATA_PORT = 0x60;
    inline constexpr std::uint8_t STATUS_PORT = 0x64;
    inline constexpr std::uint8_t CMD_PORT = 0x64;
    inline constexpr std::uint8_t SCANNING_BYTE = 0xF4;

    /**
        @todo: 
        Similar to how @qwinci did his keyboard/mouse impl. Make sure to have a virtual destructor 
        for a default class for device. This would be impamented during kmalloc/kfree
    */
    
    struct Ps2Keyboard  {
        std::uint8_t read_data();
        void write_cmd(std::uint8_t byte);
        void write_data(std::uint8_t byte);

        void on_recieve(std::uint8_t byte);

        kstd::KeyboardBuffer data;
        PS2Ack signal;
        Arch::ModifierKey key_state;
        State state_machine {};

        void ps2_keyboard_init();
    };
    
    /**
        @todo For irq12

    struct Ps2Mouse : Ps2Device {};
    */

}
