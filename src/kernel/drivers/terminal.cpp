#include "drivers/terminal.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "memory/heap.hpp"
#include "util/irq_gaurd.hpp"

namespace Drivers::Terminal {
    void start() {
        using namespace x86::Proc;
        Util::IrqRAIGaurd irq {};
        auto* thread = new Thread::ThreadBlock {"terminal", &Scheduler::g_kernel_process, run, nullptr};
        Scheduler::g_kernel_process.add_thread(thread);
        Scheduler::g_scheduler.add_ready_threads(thread);
    }

    void run(void*) {
        Detail::welcome();
        for (;;) Detail::read_and_echo();
    }
}
