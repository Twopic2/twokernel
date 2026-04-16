#include "arch/x86-64/system/exceptions.hpp"
#include "util/kernel_logger.hpp"

namespace x86::System::Idt {
    static void halt_on_exception(const char* name) {
        Util::klog("exception: %s\n", name);
        for (;;) __asm__ volatile("cli; hlt");
    }

    void x86_div_handler(ArchIrq::IrqFrame*, std::uint32_t)                         { halt_on_exception("divide error (#DE)"); }
    void x86_debug_handler(ArchIrq::IrqFrame*, std::uint32_t)                       { halt_on_exception("debug (#DB)"); }
    void x86_nmi_handler(ArchIrq::IrqFrame*, std::uint32_t)                         { halt_on_exception("NMI"); }
    void x86_breakpoint_handler(ArchIrq::IrqFrame*, std::uint32_t)                  { halt_on_exception("breakpoint (#BP)"); }
    void x86_overflow_handler(ArchIrq::IrqFrame*, std::uint32_t)                    { halt_on_exception("overflow (#OF)"); }
    void x86_bound_range_exceeded_handler(ArchIrq::IrqFrame*, std::uint32_t)        { halt_on_exception("bound range exceeded (#BR)"); }
    void x86_invalid_op_handler(ArchIrq::IrqFrame*, std::uint32_t)                  { halt_on_exception("invalid opcode (#UD)"); }
    void x86_device_not_available_handler(ArchIrq::IrqFrame*, std::uint32_t)        { halt_on_exception("device not available (#NM)"); }
    void x86_double_fault_handler(ArchIrq::IrqFrame*, std::uint32_t)                { halt_on_exception("double fault (#DF)"); }
    void x86_coproc_seg_overrun_handler(ArchIrq::IrqFrame*, std::uint32_t)          { halt_on_exception("coprocessor segment overrun"); }
    void x86_invalid_tss_handler(ArchIrq::IrqFrame*, std::uint32_t)                 { halt_on_exception("invalid TSS (#TS)"); }
    void x86_seg_not_present_handler(ArchIrq::IrqFrame*, std::uint32_t)             { halt_on_exception("segment not present (#NP)"); }
    void x86_stack_seg_fault_handler(ArchIrq::IrqFrame*, std::uint32_t)             { halt_on_exception("stack segment fault (#SS)"); }
    void x86_gp_fault_handler(ArchIrq::IrqFrame*, std::uint32_t)                    { halt_on_exception("general protection fault (#GP)"); }
    void x86_x87_floating_point_handler(ArchIrq::IrqFrame*, std::uint32_t)          { halt_on_exception("x87 floating point (#MF)"); }
    void x86_alignment_check_handler(ArchIrq::IrqFrame*, std::uint32_t)             { halt_on_exception("alignment check (#AC)"); }
    void x86_machine_check_handler(ArchIrq::IrqFrame*, std::uint32_t)               { halt_on_exception("machine check (#MC)"); }
    void x86_simd_exception_handler(ArchIrq::IrqFrame*, std::uint32_t)              { halt_on_exception("SIMD floating point (#XM)"); }
    void x86_virt_exception_handler(ArchIrq::IrqFrame*, std::uint32_t)              { halt_on_exception("virtualization (#VE)"); }
    void x86_control_protection_exception_handler(ArchIrq::IrqFrame*, std::uint32_t){ halt_on_exception("control protection (#CP)"); }
    void x86_hypervisor_injection_exception_handler(ArchIrq::IrqFrame*, std::uint32_t) { halt_on_exception("hypervisor injection (#HV)"); }
    void x86_vmm_comm_exception_handler(ArchIrq::IrqFrame*, std::uint32_t)          { halt_on_exception("VMM communication (#VC)"); }
    void x86_security_exception_handler(ArchIrq::IrqFrame*, std::uint32_t)          { halt_on_exception("security exception (#SX)"); }
    
    /// TODO: Make a special exception hanlder for pagefaults since this one is very important during paging
    void x86_pagefault_handler(ArchIrq::IrqFrame*, std::uint32_t)                   { halt_on_exception("page fault (#PF)"); }

}
