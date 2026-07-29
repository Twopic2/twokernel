#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/system/gdt.hpp"
#include "arch/x86-64/system/idt.hpp"
#include "limine/requests.hpp"
#include "memory/pmm.hpp"
#include "util/kernel_logger.hpp"
#include <arch/x86-64/proc/thread.hpp>
#include <arch/x86-64/proc/process.hpp>
#include <cstdint>

namespace x86::Proc::Thread {
    ThreadBlock::ThreadBlock(std::string_view name, Process::ProcessBlock* process, FuncPtr entry, void* _) // TODO: add args
    : m_name(name), m_entry(entry), m_process(process) {
        init_kernel_stack();

        ticks_left = TIME_SLICE_MS;
        state = ThreadState::Ready;
        registers.cs = System::Idt::code;
        registers.ss = System::Idt::data;
        registers.rflags = SET_RFLAGS;
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
        registers.rflags = SET_RFLAGS;

        total_tid++;
        thread_id = total_tid;
    }    

    void ThreadBlock::init_kernel_stack() {
       Util::IrqGaurd irq_gaurd {};
        
        pa = Memory::Pmm::alloc(4);
        if (pa == 0) {
            Util::klog_panic("Pmm::alloc(4) failed in init_kernel_stack() -- out of physical memory\n");
        }
        auto* k_stack = reinterpret_cast<std::uint64_t*>(pa + Limine::get_hhdm());
        kernel_stack = k_stack;

        auto* top = reinterpret_cast<std::byte*>(k_stack) + THREAD_KERNEL_SIZE;
        rsp0 = reinterpret_cast<std::uint64_t*>(top);

        registers.rsp = reinterpret_cast<std::uint64_t>(rsp0);
    }

    void ThreadBlock::exit() {
        Util::IrqGaurd irq {};
        state = ThreadState::Zombie;
        Memory::Pmm::free(pa, 4);
    }

    void ThreadBlock::reset() {
        ticks_left = TIME_SLICE_MS;
    }
}
