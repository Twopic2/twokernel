#pragma once

#include <std/cstdint>

namespace x86::IO {
    inline void outb(std::uint16_t port, std::uint8_t val) {
        asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
    }

    inline std::uint8_t inb(std::uint16_t port) {
        std::uint8_t val;
        asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
        return val;
    }

    inline void io_wait() {
        outb(0x80, 0);
    }
}
