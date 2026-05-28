#include <tests/ktest.hpp>
#include <arch/x86-64/system/irq.hpp>

namespace Tests {
    void run_irq_tests() {
        Util::klog("--- IRQ Tests ---\n");
        KTest::reset();

        // exception handlers 0-31 registered by idt_init()
        // spot check a few known ones
        KTEST_ASSERT(x86::System::Irq::irq_handlers[0]  != nullptr, "divide error handler registered (vector 0)");
        KTEST_ASSERT(x86::System::Irq::irq_handlers[14] != nullptr, "page fault handler registered (vector 14)");
        KTEST_ASSERT(x86::System::Irq::irq_handlers[13] != nullptr, "GPF handler registered (vector 13)");

        // vectors with no handler (nullptrs in x86_exception_handlers)
        KTEST_ASSERT(x86::System::Irq::irq_handlers[15] == nullptr, "vector 15 has no handler (reserved)");

        // deregister it
        x86::System::Irq::deregister_handler(33);
        KTEST_ASSERT(x86::System::Irq::irq_handlers[33] == nullptr,
                     "deregister_handler clears the slot");

        // timer handler registered in kmain (IRQ 0 = vector 32)
        KTEST_ASSERT(x86::System::Irq::irq_handlers[32] != nullptr,
                     "timer handler registered at vector 32");

        KTest::summary("IRQ");
    }
}
