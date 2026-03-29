#include <arch/x86-64/system/gdt.hpp>
namespace x86::System::Gdt {
    void init_gdt_entries() {
        gdt_entries[0] = 0;

        uint64_t kernel_code = 0;
        kernel_code |= 0b1011 << 8; //type of selector
        kernel_code |= 1 << 12; //not a system descriptor
        kernel_code |= 0 << 13; // Ring 0
        kernel_code |= 1 << 15; //present
        kernel_code |= 1 << 21; //long-mode segment 
        gdt_entries[1] = kernel_code << 32;

        uint64_t kernel_data = 0;
        kernel_data |= 0b0011 << 8; //type of selector
        kernel_data |= 1 << 12; //not a system descriptor
        kernel_data |= 0 << 13; // Ring 0
        kernel_data |= 1 << 15; //present
        kernel_data |= 1 << 21; //long-mode segment        
        gdt_entries[0] = kernel_data << 32;

        uint64_t user_code = kernel_code | ((uint64_t)3 << 13); // Ring 3
        gdt_entries[3] = user_code << 32;

        uint64_t user_data = kernel_data | ((uint64_t)3 << 13); // Ring 3
        gdt_entries[4] = user_data << 32;
    }

    extern "C" void gdt_flush(void *);

    void refresh() {
        struct GDTR gdtr = {
            .limit = sizeof(gdt_entries) -1,
            .address = (uint64_t)&gdt_entries,
        };

        gdt_flush(&gdtr);
    }
}
