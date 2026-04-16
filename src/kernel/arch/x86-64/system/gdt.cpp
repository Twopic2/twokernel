#include "util/kernel_logger.hpp"
#include <arch/x86-64/system/gdt.hpp>

namespace x86::System::Gdt {
    void init_gdt_entries() {
        gdt_entries[0] = 0;

        std::uint64_t kernel_code = 0;
        kernel_code |= 0b1011 << 8; //type of selector
        kernel_code |= 1 << 12; //not a system descriptor
        kernel_code |= 0 << 13; // Ring 0
        kernel_code |= 1 << 15; //present
        kernel_code |= 1 << 21; //long-mode segment 
        gdt_entries[1] = kernel_code << 32;

        std::uint64_t kernel_data = 0;
        kernel_data |= 0b0011 << 8; //type of selector
        kernel_data |= 1 << 12; //not a system descriptor
        kernel_data |= 0 << 13; // Ring 0
        kernel_data |= 1 << 15; //present
        kernel_data |= 1 << 21; //long-mode segment        
        gdt_entries[2] = kernel_data << 32;

        std::uint64_t user_code = kernel_code | ((std::uint64_t)3 << 13); // Ring 3
        gdt_entries[3] = user_code << 32;

        std::uint64_t user_data = kernel_data | ((std::uint64_t)3 << 13); // Ring 3
        gdt_entries[4] = user_data << 32;

        const auto base  = reinterpret_cast<std::uintptr_t>(&tss);
        const std::uint16_t limit = static_cast<std::uint16_t>(sizeof(tss) - 1);

        TssDescriptor desc{};
        desc.limit00 = limit;
        desc.base0   = static_cast<std::uint16_t>(base & 0xFFFF);
        desc.base1   = static_cast<std::uint8_t>(base >> 16);
        desc.access  = 0x89;
        desc.base2   = static_cast<std::uint8_t>(base >> 24);
        desc.base3   = static_cast<std::uint32_t>(base >> 32);

        const auto* seg = reinterpret_cast<std::uint64_t*>(&desc);
        gdt_entries[5] = seg[0];
        gdt_entries[6] = seg[1];

        Util::klog("gdt: entries initialized\n");
    }

    extern "C" void gdt_flush(void *);

    void refresh() {
        struct GDTR gdtr = {
            .limit = sizeof(gdt_entries) -1,
            .address = (std::uint64_t)&gdt_entries,
        };

        gdt_flush(&gdtr);
        Util::klog("gdt: loaded successfully\n");
    }
}
