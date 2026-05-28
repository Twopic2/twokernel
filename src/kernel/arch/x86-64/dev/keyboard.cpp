#include "arch/x86-64/dev/keyboard.hpp"
#include "arch/x86-64/arch/io.hpp"
#include "arch/x86-64/system/irq.hpp"
#include "util/kernel_logger.hpp"

namespace x86::Dev::Keyboard {
    void isr_keyboard(ArchIrq::IrqFrame*) {
    }

    void init() {
    }
}
