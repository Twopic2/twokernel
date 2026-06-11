#pragma once

// Irq1 Keyboard interrupt

#include "arch/x86-64/arch/arch_irq.hpp"
#include "drivers/ps2.hpp"

namespace x86::Dev::Keyboard {
    void isr_keyboard(ArchIrq::IrqFrame* frame);
    void keyboard_init();

    // The keyboard instance the ISR feeds; consumers drain its buffer
    Drivers::Ps2::Ps2Keyboard& device();

    void reset();
}
