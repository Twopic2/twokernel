#pragma once

// Irq1 Keyboard interrupt

#include "arch/x86-64/arch/arch_irq.hpp"

namespace x86::Dev::Keyboard {
    void isr_keyboard(ArchIrq::IrqFrame* frame);
    void init();
}
