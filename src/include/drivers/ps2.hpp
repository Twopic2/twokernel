#pragma once

#include "std/ring_buffer.hpp"
#include <cstdint>

/* 
So there is a controller (8042) which similar to the PIC the cpu never directly talks to it. 
This is why we have drivers which wires port 1/2 (0x60/0x64).

Then the controller talks to the keyboard through with the irq1 with on recieve. 

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
    inline constexpr std::uint8_t DISABLE_SCANNING = 0xF5;
    inline constexpr std::uint8_t RESEND = 0xFE;
    inline constexpr std::uint8_t ACK = 0xFA;
    inline constexpr std::uint8_t DISABLE_PORT1 = 0xAD;
    inline constexpr std::uint8_t ENABLE_PORT1 = 0xAE;
    inline constexpr std::uint8_t CPU_RESET = 0xFE;
    inline constexpr std::uint8_t MAX_RETRY = 3;

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

        std::uint8_t last_data {};
        std::uint8_t nack_count {};
        kstd::KeyboardBuffer data;
        PS2Ack signal {};
        State state_machine {};

        void ps2_keyboard_init();
    };
    
    /**
        @todo For irq12

    struct Ps2Mouse : Ps2Device {};
    */

}
