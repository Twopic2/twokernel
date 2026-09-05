#include "arch/x86-64/arch/io.hpp"
#include <cstdint>
#include <drivers/pit.hpp>

namespace Drivers::Pit {
    void set_reload_access(std::uint8_t channel_cmd, std::uint8_t access, std::uint8_t pit_mode) {
        x86::IO::outb(COMMAND, channel_cmd | access | pit_mode);
    }

    void set_reload_value(std::size_t hz, std::uint8_t access) {
        std::uint16_t divisor = HZ / hz;

        if (access == ACCESSLH) {
            x86::IO::outb(CHANNEL0, divisor & 0xFF);
            x86::IO::outb(CHANNEL0, (divisor >> 8) & 0xFF);
        } else if (access == ACCESSLO) {
            x86::IO::outb(CHANNEL0, divisor & 0xFF);
        } else if (access == ACCESSHI) {
            x86::IO::outb(CHANNEL0, (divisor >> 8) & 0xFF);
        }
    }

    void init_pit(std::uint8_t channel_cmd, std::uint8_t access, std::uint8_t pit_mode) {
        set_reload_access(channel_cmd, access, pit_mode);
        set_reload_value(FREQUENCY, access);
    }

    std::size_t read_pit_count() {
        std::size_t count = 0;

        x86::IO::outb(COMMAND, CHANNEL0_CMD | LATCH);

        count  = x86::IO::inb(CHANNEL0);
        count |= x86::IO::inb(CHANNEL0) << 8;

        return count;
    }

    void increase_tick() {
        tick += 1;
        pit_clocks += RELOAD;
    }

    void reset_tick() {
        tick = 0;
        pit_clocks = 0;
    }

    void increase_time() {
        seconds       = pit_clocks / HZ;
        milli_seconds = get_time_milliseconds();
    }

    std::uint64_t get_time_seconds() {
        return pit_clocks / HZ;
    }

    std::uint64_t get_time_milliseconds() {
        return (tick * 1000) / FREQUENCY;
    }
}
