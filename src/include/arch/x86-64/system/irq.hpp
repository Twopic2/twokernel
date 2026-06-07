#pragma once

#include <arch/x86-64/arch/arch_irq.hpp>
#include <array>
#include <cstdint>
#include <std/cstdint>

/// CREDIT: 
// Thanks to Qwinci for the isr.s dump. His stubs.s helped me a lot

/// TODO: Should change the Pic to Apicx2
/// CREDIT: https://wiki.osdev.org/8259_PIC
/// Big thanks to the osdev wiki for helping me set up the 8259 Pic. 
///  "The function of the 8259A is to manage hardware interrupts and send them to 
/// the appropriate system interrupt.This allows the system to respond to devices needs  
///  without loss of time (from polling the device, for instance)."

namespace x86::System::Irq {
    namespace Pic {
        constexpr std::uint16_t MASTER_CMD  = 0x20;
        constexpr std::uint16_t MASTER_DATA = 0x21;
        constexpr std::uint16_t SLAVE_CMD   = 0xA0;
        constexpr std::uint16_t SLAVE_DATA  = 0xA1;

        constexpr std::uint8_t BEGIN_INIT = 0x11;

        constexpr std::uint8_t ICW3_MASTER = 0x04;  
        constexpr std::uint8_t ICW3_SLAVE  = 0x02;  

        constexpr std::uint8_t ICW4_8086 = 0x01;

        constexpr std::uint8_t EOI = 0x20;

        constexpr std::uint8_t IRQ_MASTER = 0x20;
        constexpr std::uint8_t IRQ_SLAVE = 0x28;
    }
    
    using IrqHandler = void(*)(ArchIrq::IrqFrame*);

    inline std::array<IrqHandler, 256> irq_handlers {};

    void eoi(const std::uint8_t vector);
    void pic_remap();
    void pic_disable();
    void irq_mask(std::uint8_t irq_line);
    void irq_unmask(std::uint8_t irq_line);

    void hardware_interrupt_dispatch(std::uint8_t irq_line, ArchIrq::IrqFrame* frame);
    void register_exception_handler(const std::uint8_t n, IrqHandler handler);
    void deregister_handler(std::uint8_t n);
}
