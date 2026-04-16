#include "arch/x86-64/arch.hpp"
#include "arch/x86-64/system/gdt.hpp"
#include "arch/x86-64/system/idt.hpp"
#include "arch/x86-64/system/irq.hpp"
#include "util/kernel_logger.hpp"

namespace x86 {
    bool inital_init() {
        System::Gdt::init_gdt_entries();
        System::Gdt::refresh();
        return true;
    }

    void exception_load() {
        System::Irq::pic_remap();

        Util::klog("Masking Irq\n");
        for (int i {0}; i < 16; i++) {
            System::Irq::irq_mask(i);
        }
        Util::klog("Finished irq mask\n");

        Util::klog("Loading Interrupts/Idt\n");
        System::Idt::idt_init();
        System::Idt::idtr_asm_load();
    }
}
