#include "arch/x86-64/dev/timer.hpp"
#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "drivers/pit.hpp"
#include "util/kernel_logger.hpp"
#include "util/ansi.hpp"
#include "arch/x86-64/system/irq.hpp"

namespace x86::Dev::Timer {
    void isr_timer(ArchIrq::IrqFrame* frame) {
        Drivers::Pit::increase_tick();
        Drivers::Pit::increase_time();
      
        Proc::Scheduler::g_scheduler.pit_irq_handler(frame);
        
        System::Irq::eoi(0);
    }

    void timer_init() {
        Util::klog("%s%s[init] timer: initializing PIT%s\n", BOLD, RED, RESET);

        Drivers::Pit::init_pit(Drivers::Pit::CHANNEL0_CMD,
        Drivers::Pit::ACCESSLH, Drivers::Pit::MODE3);

        Drivers::Pit::reset_tick();
        System::Irq::irq_unmask(0);

        Util::klog("timer: IRQ0 unmasked\n");
    }

    void sleep(std::uint64_t milli) {
        std::uint64_t target = Drivers::Pit::get_time_milliseconds() + milli;
        while (Drivers::Pit::get_time_milliseconds() < target) {
            asm volatile("hlt");
        }
    }
}
