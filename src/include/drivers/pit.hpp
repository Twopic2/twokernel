#pragma once

/// Credit: https://wiki.osdev.org/Programmable_Interval_Timer#Mode_0_%E2%80%93_Interrupt_On_Terminal_Count
// The Programmable Interval Timer (PIT) chip (Intel 8253/8254) basically consists of an 
// oscillator, a prescaler and 3 independent frequency dividers. 

#include <cstdint>
#include <arch/x86-64/arch/io.hpp>

namespace Drivers::Pit {
    inline constexpr std::size_t HZ = 1193182;
    inline constexpr std::size_t FREQUENCY = 1000;
    inline constexpr std::uint16_t RELOAD  = HZ / FREQUENCY;   
    
    // Channels
    inline constexpr std::uint8_t CHANNEL0 = 0x40; 
    inline constexpr std::uint8_t CHANNEL1 = 0x41;
    inline constexpr std::uint8_t CHANNEL2 = 0x42; 
    inline constexpr std::uint8_t COMMAND = 0x43;

    // Select Channels
    inline constexpr std::uint8_t CHANNEL0_CMD = (0b00 << 6); 
    inline constexpr std::uint8_t CHANNEL1_CMD = (0b01 << 6);
    inline constexpr std::uint8_t CHANNEL2_CMD = (0b10 << 6);
    inline constexpr std::uint8_t READBACK_CMD = (0b11 << 6);

    // Access
    inline constexpr std::uint8_t LATCH = (0b00 << 4);
    inline constexpr std::uint8_t ACCESSLO = (0b01 << 4);
    inline constexpr std::uint8_t ACCESSHI = (0b10 << 4);
    inline constexpr std::uint8_t ACCESSLH = (0b11 << 4);

    // Pit Mode
    inline constexpr std::uint8_t MODE0 = (0b000 << 1); // Terminal
    inline constexpr std::uint8_t MODE1 = (0b001 << 1); // One-shot
    inline constexpr std::uint8_t MODE2 = (0b010 << 1); // rate 
    inline constexpr std::uint8_t MODE3 = (0b011 << 1); // square
    inline constexpr std::uint8_t MODE4 = (0b100 << 1); // software strobe
    inline constexpr std::uint8_t MODE5 = (0b101 << 1); // hardware strobe
    
    inline volatile std::uint64_t pit_clocks = 0;
    inline volatile std::uint64_t tick = 0;

    void init_pit(std::uint8_t commands, std::uint8_t access, std::uint8_t pit_mode);

    void reset_timer();
    void set_reload_value(std::size_t count, std::uint8_t access);
    std::size_t read_pit_count();
    void increase_tick();
    void reset_tick();
}
