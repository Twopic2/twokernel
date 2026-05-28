#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include <cstdint>

// Irq0 hardware interrupt

namespace x86::Dev::Timer {
    void isr_timer(ArchIrq::IrqFrame* frame);
    void timer_init();
    void schedule();
    void sleep(std::uint64_t milli);
}
