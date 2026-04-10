#include "arch/x86-64/system/idt.hpp"
#include <cstdint>

/* 
IRQ Mapping

IRQ	        INT
0x0-0x7	    0x8-0xF
0x8-0xF	    0x70-0x78
*/

namespace x86::System::Idt {
    
    namespace {
        // Credit to ilobilo
        constexpr std::array<std::string_view, 32> exception_messages {
            "division by zero", "debug",
            "non-maskable interrupt",
            "breakpoint", "detected overflow",
            "out-of-bounds", "invalid opcode",
            "no coprocessor", "double fault",
            "coprocessor segment overrun",
            "bad TSS", "segment not present",
            "stack fault", "general protection fault",
            "page fault", "unknown interrupt",
            "coprocessor fault", "alignment check",
            "machine check", "reserved",
            "reserved", "reserved", "reserved",
            "reserved", "reserved", "reserved",
            "reserved", "reserved", "reserved",
            "reserved", "reserved", "reserved"
        };
    }

    void set_idt_entries(std::uint8_t index, void* handler, std::uint8_t ist, std::uint8_t dpl) {
        auto address = reinterpret_cast<std::uintptr_t>(handler);

        auto& entry = idt_table.at(index);

        entry.offset0 = address;
        entry.offset1 = static_cast<std::uint16_t>(address >> 16);
        entry.offset2 = static_cast<std::uint32_t>(address >> 32);
        
        entry.selector = code;
        entry.ist_flags = (ist & 0b111) | (0xE << 8) | (dpl & 0b11) << 13 | 1 << 15;
    }

    extern "C" void interrupt_handler();

    void idt_init() {
        for (std::uint32_t i {0}; i < idt_table.size(); i++) {
        }

        for (std::uint32_t i {0}; i < exception_messages.size(); i++) {

        }

        idtr_asm_load();
    }

    void idtr_asm_load() {
        idtr idtr {
            .limit = sizeof(idt_table) - 1,
            .base = reinterpret_cast<std::uint64_t>(idt_table.data())
        };
        asm volatile("cli; lidt %0; sti" : : "m"(idtr));
    }
}