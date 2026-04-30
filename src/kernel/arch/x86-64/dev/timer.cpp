#include "arch/x86-64/dev/timer.hpp"
#include "util/kernel_logger.hpp"

namespace x86::Dev::Timer {
    void x86_IRQ0_timer(ArchIrq::IrqFrame*) {
        Util::klog("timer tick\n");
    }
}
