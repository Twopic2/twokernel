#include "arch/x86-64/system/idt.hpp"
#include "arch/x86-64/system/exceptions.hpp"
#include "arch/x86-64/system/irq.hpp"
#include "util/kernel_logger.hpp"
#include <cstdint>

/*
IRQ Mapping

IRQ	        INT
0-7	        32-39
8-15	    40–47            
*/

namespace x86::System::Idt {
    constexpr std::array<Irq::IrqHandler, 32> x86_exception_hanlders = {
        x86_div_handler,
        x86_debug_handler,
        x86_nmi_handler,
        x86_breakpoint_handler,
        x86_overflow_handler,
        x86_bound_range_exceeded_handler,
        x86_invalid_op_handler,
        x86_device_not_available_handler,
        x86_double_fault_handler,
        x86_coproc_seg_overrun_handler,
        x86_invalid_tss_handler,
        x86_seg_not_present_handler,
        x86_stack_seg_fault_handler,
        x86_gp_fault_handler,
        x86_pagefault_handler,
        nullptr,
        x86_x87_floating_point_handler,
        x86_alignment_check_handler,
        x86_machine_check_handler,
        x86_simd_exception_handler,
        x86_virt_exception_handler,
        x86_control_protection_exception_handler,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        x86_hypervisor_injection_exception_handler,
        x86_vmm_comm_exception_handler,
        x86_security_exception_handler,
        nullptr,
    };

    void set_idt_entries(std::uint8_t index, void* handler, std::uint8_t ist, std::uint8_t dpl) {
        auto address = reinterpret_cast<std::uintptr_t>(handler);

        auto& entry = idt_table[index];

        entry.offset0 = address;
        entry.offset1 = static_cast<std::uint16_t>(address >> 16);
        entry.offset2 = static_cast<std::uint32_t>(address >> 32);

        entry.selector = code;
        entry.ist_flags = (ist & 0b111) | (0xE << 8) | (dpl & 0b11) << 13 | 1 << 15;
    }

    void idt_init() {
        for (std::uint32_t i {0}; i < idt_table.size(); i++) {
            set_idt_entries(i, irq_stubs[i], 0x8, 0);
        }
        
        Util::klog("idt: stubs installed for all %u vectors\n", idt_table.size());

        for (std::uint32_t i {0}; i < x86_exception_hanlders.size(); i++) {
            if (x86_exception_hanlders[i] != nullptr) {
                Irq::register_exception_handler(i, x86_exception_hanlders[i]);
            }
        }
        Util::klog("idt: exception handlers registered\n");
    }

    void idtr_asm_load() {
        idtr idtr {
            .limit = sizeof(idt_table) - 1,
            .base = reinterpret_cast<std::uint64_t>(idt_table.data())
        };
        asm volatile("cli; lidt %0; sti" : : "m"(idtr));
        Util::klog("idt: loaded successfully\n");
    }
}
