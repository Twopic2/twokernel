#pragma once

#include <std/array>
#include <std/string_view>
#include <std/cstdint>
#include <std/cstddef>

// 0b1110 or 0xE: 64-bit Interrupt Gate
// 0b1111 or 0xF: 64-bit Trap Gate

namespace x86::System::Idt {
    struct [[gnu::packed]] idtr {
        std::uint16_t limit;
        std::uint64_t base;
    };

    struct [[gnu::packed]] IdtEntry {
        std::uint16_t offset0;
        std::uint16_t selector;
        std::uint16_t ist_flags;
        std::uint16_t offset1;
        std::uint32_t offset2;
        std::uint32_t resered;
    };

    extern void* irq_stubs[];

    // GDT code segment
    constexpr std::uint8_t code = 0x08;
    constexpr std::size_t int_vector {256};
    
    inline std::array<IdtEntry, int_vector> idt_table;

    void set_idt_entries(std::uint8_t index, void* handler, std::uint8_t ist, std::uint8_t dpl);

    void idt_init();
    void idtr_asm_load();
}