#pragma once

#include <arch/x86-64/arch/arch_irq.hpp>
#include <std/cstdint>

/// So exceptions also known as traps (UwU). Are program triggers 

/**
 *!For user-space processes: Delivers SIGSEGV signal to the thread
 *!For kernel faults: Panics the system
 */

namespace x86::System::Idt {
    void x86_div_handler(ArchIrq::IrqFrame* frame);
    void x86_debug_handler(ArchIrq::IrqFrame* frame);
    void x86_nmi_handler(ArchIrq::IrqFrame* frame);
    void x86_breakpoint_handler(ArchIrq::IrqFrame* frame);
    void x86_overflow_handler(ArchIrq::IrqFrame* frame);
    void x86_bound_range_exceeded_handler(ArchIrq::IrqFrame* frame);
    void x86_invalid_op_handler(ArchIrq::IrqFrame* frame);
    void x86_device_not_available_handler(ArchIrq::IrqFrame* frame);
    void x86_double_fault_handler(ArchIrq::IrqFrame* frame);
    void x86_coproc_seg_overrun_handler(ArchIrq::IrqFrame* frame);
    void x86_invalid_tss_handler(ArchIrq::IrqFrame* frame);
    void x86_seg_not_present_handler(ArchIrq::IrqFrame* frame);
    void x86_stack_seg_fault_handler(ArchIrq::IrqFrame* frame);
    void x86_gp_fault_handler(ArchIrq::IrqFrame* frame);
    void x86_x87_floating_point_handler(ArchIrq::IrqFrame* frame);
    void x86_alignment_check_handler(ArchIrq::IrqFrame* frame);
    void x86_machine_check_handler(ArchIrq::IrqFrame* frame);
    void x86_simd_exception_handler(ArchIrq::IrqFrame* frame);
    void x86_virt_exception_handler(ArchIrq::IrqFrame* frame);
    void x86_control_protection_exception_handler(ArchIrq::IrqFrame* frame);
    void x86_hypervisor_injection_exception_handler(ArchIrq::IrqFrame* frame);
    void x86_vmm_comm_exception_handler(ArchIrq::IrqFrame* frame);
    void x86_security_exception_handler(ArchIrq::IrqFrame* frame);

    /* 
    Demand Paging: Accessing the page that is not currently loaded in the memory (RAM).

    Invalid Memory Access, it occurs when a program tries to access memory 
    beyond its allocated boundaries or not allocated.
   
    Process Violation: when a process tries to write to a read-only page 
    or otherwise violates memory protection rules.
    */    
    void x86_pagefault_handler(ArchIrq::IrqFrame* frame);
}
