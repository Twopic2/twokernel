#include "arch/x86-64/system/irq.hpp"
#include "arch/x86-64/arch/io.hpp"
#include "arch/x86-64/system/idt.hpp"
#include "util/kernel_logger.hpp"
#include <cstdint>

namespace x86::System::Irq {
    void pic_remap(std::uint8_t master_offset, std::uint8_t slave_offset) {
        auto mask1 = IO::inb(Pic::MASTER_DATA);
        auto mask2 = IO::inb(Pic::SLAVE_DATA);

        // starts the initialization sequence (in cascade mode)
        IO::outb(Pic::MASTER_CMD, Pic::ICW1_INIT | Pic::ICW1_ICW4);
        IO::io_wait();
        IO::outb(Pic::SLAVE_CMD, Pic::ICW1_INIT | Pic::ICW1_ICW4);
        IO::io_wait();
        
        // ICW2: Master PIC vector offset
        IO::outb(Pic::MASTER_DATA, master_offset);
        IO::io_wait();
         // ICW2: Slave PIC vector offset
        IO::outb(Pic::SLAVE_DATA, slave_offset);
        IO::io_wait();
        
         // ICW3: tell Master PIC that there is a slave PIC at IRQ2
        IO::outb(Pic::MASTER_DATA, Pic::ICW3_MASTER);
        IO::io_wait();
        // ICW3: tell Slave PIC its cascade identity (0000 0010)
        IO::outb(Pic::SLAVE_DATA, Pic::ICW3_SLAVE);
        IO::io_wait();

        // ICW4: have the PICs use 8086 mode (and not 8080 mode)
        IO::outb(Pic::MASTER_DATA, Pic::ICW4_8086);
        IO::io_wait();
        IO::outb(Pic::SLAVE_DATA, Pic::ICW4_8086);
        IO::io_wait();

        // Restore saved masks
        IO::outb(Pic::MASTER_DATA, mask1);
        IO::outb(Pic::SLAVE_DATA, mask2);
    }

    void pic_disable() {
        IO::outb(Pic::MASTER_CMD, 0xFF);
        IO::outb(Pic::SLAVE_CMD, 0xFF);
    }

    void eoi(std::uint8_t vector) {
        if (vector >= 8) {
            IO::outb(Pic::SLAVE_CMD, Pic::EOI);
        }
        IO::outb(Pic::MASTER_CMD, Pic::EOI);
    }

    void irq_mask(std::uint8_t irq_line) {
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

    void register_exception_handler(std::uint8_t n, IrqHandler handler) {
        if (n >= 32) {
            Util::klog("out of bounds exception vector %u: must be 0-31\n", n);
            for (;;) __asm__ volatile ("cli; hlt");
        }

        irq_handlers[n] = handler;
    }

    extern "C" [[gnu::used]] void arch_interrupt_handler(ArchIrq::IrqFrame* frame, std::uint32_t vector) {
        if (auto h = irq_handlers[vector]) {
            h(frame, vector);
        } else {
            Util::klog("unhandled CPU exception\n");
            for (;;) __asm__ volatile ("cli; hlt");
        }

        if (vector >= 32 && vector < 48) {
            eoi(vector - 0x20);
        }
    }
}
