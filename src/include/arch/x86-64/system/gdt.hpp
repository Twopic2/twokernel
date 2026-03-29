#pragma once

#include <std/cstdint>

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
    std::uint64_t gdt_entries[7];

    struct GDTR {
        std::uint16_t limit;
        std::uint64_t address;
    } __attribute__((packed));

    void init_gdt_entries();
    void refresh();
}
