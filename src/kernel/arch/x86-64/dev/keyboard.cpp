#include "arch/x86-64/dev/keyboard.hpp"
#include "arch/x86-64/arch/io.hpp"
#include "arch/x86-64/system/irq.hpp"
#include "util/kernel_logger.hpp"

namespace x86::Dev::Keyboard {
    void isr_keyboard(ArchIrq::IrqFrame*) {
        std::uint8_t scancode = IO::inb(0x60);
        Util::klog("kbd: scancode=0x%x\n", scancode);
    }

    void init() {
        System::Irq::register_irq_hanlder(1, isr_keyboard, 0x20);
        System::Irq::irq_unmask(1);
        Util::klog("kbd: initialized on IRQ1\n");
    }
}
