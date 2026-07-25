#include <tests/ktest.hpp>
#include <arch/x86-64/system/irq.hpp>

namespace Tests {
    TEST_CASE("idt_init registers the exception handlers", "[irq]") {
        // spot check a few known ones
        CHECK(x86::System::Irq::irq_handlers[0]  != nullptr); // divide error
        CHECK(x86::System::Irq::irq_handlers[14] != nullptr); // page fault
        CHECK(x86::System::Irq::irq_handlers[13] != nullptr); // GPF

        // vectors with no handler (nullptrs in x86_exception_handlers)
        CHECK(x86::System::Irq::irq_handlers[15] == nullptr); // reserved
    }

    TEST_CASE("deregister_handler clears the slot", "[irq]") {
        x86::System::Irq::deregister_handler(33);
        CHECK(x86::System::Irq::irq_handlers[33] == nullptr);
    }

    TEST_CASE("timer handler is registered at vector 32", "[irq]") {
        // registered in kmain (IRQ 0 = vector 32)
        CHECK(x86::System::Irq::irq_handlers[32] != nullptr);
    }
}
