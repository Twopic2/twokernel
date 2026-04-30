#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"

namespace x86::Dev::Timer {
    void x86_IRQ0_timer(ArchIrq::IrqFrame* frame);
}
