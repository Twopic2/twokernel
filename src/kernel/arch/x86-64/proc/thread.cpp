#include "arch/x86-64/system/gdt.hpp"
#include "arch/x86-64/system/idt.hpp"
#include "limine/requests.hpp"
#include "memory/pmm.hpp"
#include <arch/x86-64/proc/thread.hpp>
#include <arch/x86-64/proc/process.hpp>
#include <cstdint>

namespace x86::Proc::Thread {
    ThreadBlock::ThreadBlock(const char* name, Process::ProcessBlock* process, FuncPtr entry) 
    : m_entry(entry), m_process(process), m_name(name) {
        init_kernel_stack();

        ticks_left = TIME_SLICE_MS;
        state = ThreadState::Ready;
        registers.cs = System::Idt::code;
        registers.ss = System::Idt::data;
        registers.rflags = 0x202;
        registers.rip = reinterpret_cast<std::uint64_t>(entry);

        total_tid++;
        thread_id = total_tid;
    }

    ThreadBlock::ThreadBlock() {
        init_kernel_stack();

        ticks_left = TIME_SLICE_MS;
        state = ThreadState::Ready;
        registers.cs = System::Idt::code;
        registers.ss = System::Idt::data;
        registers.rflags = 0x202;

        total_tid++;
        thread_id = total_tid;
    }    

    void ThreadBlock::init_kernel_stack() {
       Util::IrqGaurd irq_gaurd {};
        
        pa = Memory::Pmm::alloc(4);
        auto* k_stack = reinterpret_cast<std::uint64_t*>(pa + Limine::get_hhdm());
        kernel_stack = k_stack;

        auto* top = reinterpret_cast<std::byte*>(k_stack) + 4 * Memory::Vmm::page_size;
        rsp0 = reinterpret_cast<std::uint64_t*>(top);

        registers.rsp = reinterpret_cast<std::uint64_t>(rsp0);
        kernel_rsp = rsp0;
    }

    void ThreadBlock::exit() {
        Util::IrqGaurd irq {};
        Memory::Pmm::free(pa, 4);
    }
}
