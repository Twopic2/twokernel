#pragma once

#include <std/cstdint>
#include <std/cstddef>

// 0b1110 or 0xE: 64-bit Interrupt Gate
// 0b1111 or 0xF: 64-bit Trap Gate

namespace x86::System::Idt {
    struct [[gnu::packed]] idtr {
        uint16_t limit;
        uint64_t base;
        
        void load_as() const {
            asm volatile  ("cli; lidt %0" :: "memory"(*this));
        }
    };

    struct [[gnu::packed]] InterDescriptor {
        std::uint16_t offset0;
        std::uint16_t selector;
        std::uint8_t ist;
        std::uint8_t typeattr;
        std::uint16_t offset1;
        std::uint32_t offset2;
        std::uint32_t zero;
    };

    static struct InterDescriptor ist {};
    constexpr std::size_t int_vec = {256};

    void set_ist_entries(InterDescriptor& id, void *isr, std::uint8_t p_typeattr = 0x8E, std::uint8_t _ist = 0);
    
    void idt_init();
}