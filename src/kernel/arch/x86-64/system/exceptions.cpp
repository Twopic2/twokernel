#include "arch/x86-64/system/exceptions.hpp"
#include "util/kernel_logger.hpp"
#include "util/ansi.hpp"
#include <cstdint>

using namespace Util::Ansi;

namespace x86::System::Idt {
    static void halt_on_exception(const char* name, ArchIrq::IrqFrame* frame) {
        Util::klog("%s%s[#EXCEPTION] %s%s\n", BOLD, RED, name, RESET);
        Util::klog("%s  rip=%llx  cs=%llx  rflags=%llx%s\n", RED, frame->rip, frame->cs, frame->rflags, RESET);
        Util::klog("%s  rsp=%llx  ss=%llx  error=%llx%s\n", RED, frame->rsp, frame->ss, frame->error, RESET);
        Util::klog("%s  rax=%llx  rbx=%llx  rcx=%llx  rdx=%llx%s\n", RED, frame->rax, frame->rbx, frame->rcx, frame->rdx, RESET);
        Util::klog("%s  rsi=%llx  rdi=%llx  rbp=%llx%s\n", RED, frame->rsi, frame->rdi, frame->rbp, RESET);
        Util::klog("%s  r8=%llx   r9=%llx   r10=%llx  r11=%llx%s\n", RED, frame->r8, frame->r9, frame->r10, frame->r11, RESET);
        Util::klog("%s  r12=%llx  r13=%llx  r14=%llx  r15=%llx%s\n", RED, frame->r12, frame->r13, frame->r14, frame->r15, RESET);
        for (;;) __asm__ volatile("cli; hlt");
    }

    void x86_div_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("divide error (#DE)", frame); }
    void x86_debug_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("debug (#DB)", frame); }
    void x86_nmi_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("NMI", frame); }
    void x86_breakpoint_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("breakpoint (#BP)", frame); }
    void x86_overflow_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("overflow (#OF)", frame); }
    void x86_bound_range_exceeded_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("bound range exceeded (#BR)", frame); }
    void x86_invalid_op_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("invalid opcode (#UD)", frame); }
    void x86_device_not_available_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("device not available (#NM)", frame); }
    void x86_double_fault_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("double fault (#DF)", frame); }
    void x86_coproc_seg_overrun_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("coprocessor segment overrun", frame); }
    void x86_invalid_tss_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("invalid TSS (#TS)", frame); }
    void x86_seg_not_present_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("segment not present (#NP)", frame); }
    void x86_stack_seg_fault_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("stack segment fault (#SS)", frame); }
    void x86_gp_fault_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("general protection fault (#GP)", frame); }
    void x86_x87_floating_point_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("x87 floating point (#MF)", frame); }
    void x86_alignment_check_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("alignment check (#AC)", frame); }
    void x86_machine_check_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("machine check (#MC)", frame); }
    void x86_simd_exception_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("SIMD floating point (#XM)", frame); }
    void x86_virt_exception_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("virtualization (#VE)", frame); }
    void x86_control_protection_exception_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("control protection (#CP)", frame); }
    void x86_hypervisor_injection_exception_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("hypervisor injection (#HV)", frame); }
    void x86_vmm_comm_exception_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("VMM communication (#VC)", frame); }
    void x86_security_exception_handler(ArchIrq::IrqFrame* frame) { halt_on_exception("security exception (#SX)", frame); }

    /// TODO: Make a special exception hanlder for pagefaults since this one is very important during paging
    void x86_pagefault_handler(ArchIrq::IrqFrame* frame) {
        std::uint64_t pg_address;
        asm volatile("mov %%cr2, %0" : "=r" (pg_address));

        halt_on_exception("Page Fault (#PF)", frame);
    }
}
