#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"

// Irq0 hardware interrupt

namespace x86::Dev::Timer {


    
    void isr_timer(ArchIrq::IrqFrame* frame);

    void timer_init();
}
