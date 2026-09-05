#include "arch/x86-64/system/irq.hpp"
#include "arch/x86-64/arch/io.hpp"
#include "arch/x86-64/dev/keyboard.hpp"
#include "arch/x86-64/dev/timer.hpp"
#include "util/kernel_logger.hpp"
#include <cstdint>

namespace x86::System::Irq {
    void pic_remap() {
        // starts the initialization sequence (in cascade mode)
        IO::outb(Pic::MASTER_CMD, Pic::BEGIN_INIT);
        IO::outb(Pic::SLAVE_CMD, Pic::BEGIN_INIT);

        // ICW2: Master PIC vector offset
        IO::outb(Pic::MASTER_DATA, Pic::IRQ_MASTER);
         // ICW2: Slave PIC vector offset
        IO::outb(Pic::SLAVE_DATA, Pic::IRQ_SLAVE);
        
         // ICW3: tell Master PIC that there is a slave PIC at IRQ2
        IO::outb(Pic::MASTER_DATA, Pic::ICW3_MASTER);
        // ICW3: tell Slave PIC its cascade identity (0000 0010)
        IO::outb(Pic::SLAVE_DATA, Pic::ICW3_SLAVE);

        // ICW4: have the PICs use 8086 mode (and not 8080 mode)
        IO::outb(Pic::MASTER_DATA, Pic::ICW4_8086);
        IO::outb(Pic::SLAVE_DATA, Pic::ICW4_8086);

        Util::klog("irq: PIC remapped (master=0x20, slave=0x28)\n");
    }

    void pic_disable() {
        IO::outb(Pic::MASTER_CMD, 0xFF);
        IO::outb(Pic::SLAVE_CMD, 0xFF);
    }

    void eoi(const std::uint8_t vector) {
        if (vector >= 8) {
            IO::outb(Pic::SLAVE_CMD, 0x20);
        }
        IO::outb(Pic::MASTER_CMD, 0x20);
    }

    void irq_mask(std::uint8_t irq_line) {
        Util::klog("Masking Irq: %u\n", irq_line);

        std::uint16_t port {};
        std::uint8_t value {};

        // How the two Pic chips are wired together 
        if (irq_line < 8) {
            port = Pic::MASTER_DATA;            
        } else {
            port = Pic::SLAVE_DATA;
            irq_line -= 8;
        }

        value = IO::inb(port) | (1 << irq_line);
        IO::outb(port, value);
    }

    void irq_unmask(std::uint8_t irq_line) {
        std::uint16_t port {};
        std::uint8_t value {};

        // How the two Pic chips are wired together
        if (irq_line < 8) {
            port = Pic::MASTER_DATA;
        } else {
            port = Pic::SLAVE_DATA;
            irq_line -= 8;
        }

        value = IO::inb(port) & ~(1 << irq_line);
        IO::outb(port, value);
    }

    void register_exception_handler(const std::uint8_t n, IrqHandler handler) {
        if (n >= 32) {
            Util::klog("irq: out of bounds exception vector %u: must be 0-31\n", n);
            for (;;) __asm__ volatile ("cli; hlt");
        }

        irq_handlers[n] = handler;
    }
    
    void deregister_handler(std::uint8_t n) {
        irq_handlers[n] = nullptr;
    }

    extern "C" [[gnu::used]] void arch_interrupt_handler(ArchIrq::IrqFrame* frame, std::uint32_t vector) {
        //Util::klog("init arch_interrupt_handler"); 
        // CPU exception
        if (vector < 32) {
            

            if (auto h = irq_handlers[vector]) {
                h(frame);
            } else {
                Util::klog("unhandled CPU exception %u\n", vector);
                for (;;) {
                    asm volatile ("cli; hlt");
                }
            }
        } else if (vector < 48) { // Hardware IRQ
            auto new_vector = vector - 32;

            hardware_interrupt_dispatch(new_vector, frame);
        }
    }

    void hardware_interrupt_dispatch(std::uint8_t irq_line, ArchIrq::IrqFrame *frame) {
        switch (irq_line) {
            case 0: 
                x86::Dev::Timer::isr_timer(frame);
                break;
            case 1:
                x86::Dev::Keyboard::isr_keyboard(frame);
                break;
        }
    }
}
