#pragma once

#include <cstdint>

/// RESOURCES: 
/// https://0xc0ffee.netlify.app/osdev/17-tss#creating-a-tss

/* 
What to put inside a long mode GDT

Null descriptor 

Kernel Code Segment
Selector 0x08: kernel code (64-bit, ring 0)

Kernel Data Segment 
Selector 0x10: kernel data (64-bit)

User Data Segment
Selector 0x20: user data (64-bit)

User Code Segment 
Selector 0x18: user code (64-bit, ring 3)

TDD
*/

namespace x86::System::Gdt {
    static std::uint64_t gdt_entries[7];

    struct GDTR {
        std::uint16_t limit;
        std::uint64_t address;
    } __attribute__((packed));

    struct [[gnu::packed]] TssSegments {
        std::uint32_t reserved0;
        std::uint64_t rsp0; // Context switching 
        std::uint64_t rsp1;
        std::uint64_t rsp2;
        std::uint64_t reserved1;
        // Interrupt Stack Table IST1-7 used by the IDT for interrupts 
        std::uint64_t ist1;
        std::uint64_t ist2; 
        std::uint64_t ist3;
        std::uint64_t ist4; 
        std::uint64_t ist5; 
        std::uint64_t ist6;
        std::uint64_t ist7;
        std::uint64_t reserved2;
        std::uint16_t reserved3;
        std::uint16_t iomap_base;
    };

    /* So TssDescriptor takes up 128 bits and the gdt_entries need a low and high bit */
    struct [[gnu::packed]] TssDescriptor {
        std::uint16_t limit00;
        std::uint16_t base0;
        std::uint8_t base1;
        std::uint8_t access;
        std::uint8_t limit1 : 4;
        std::uint8_t flags : 4;
        std::uint8_t base2;
        std::uint32_t base3;
        std::uint32_t reserved;
    };

    inline struct TssSegments tss;

    void init_gdt_entries();
    void refresh();
}
