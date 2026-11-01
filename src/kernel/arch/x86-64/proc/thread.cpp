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
    ThreadBlock::ThreadBlock(std::string_view name, Process::ProcessBlock* process, FuncPtr entry, void* args)
    : m_name(name), m_entry(entry), m_process(process) {
        init_kernel_stack();

        scheduled_time = DEFAULT_SCHEDULE_TIME_MS;
        state = ThreadState::Ready;
        
        if (process->is_user) {
            registers.cs = System::Idt::USER_CODE | 3;
            registers.ss = System::Idt::USER_DATA | 3;
        } else {
            registers.cs = System::Idt::KERNEL_CODE;
            registers.ss = System::Idt::KERNEL_DATA;
            // iret enters the function directly: reserve its return-address slot.
            registers.rsp -= sizeof(std::uint64_t);
            *reinterpret_cast<std::uint64_t*>(registers.rsp) = 0;
        }
            
        registers.rflags = SET_RFLAGS;
        registers.rip = reinterpret_cast<std::uint64_t>(entry);
        registers.rdi = reinterpret_cast<std::uintptr_t>(args);

        g_total_tid++;
        thread_id = g_total_tid;
    }

    /* Default kernel thread */
    ThreadBlock::ThreadBlock() {
        init_kernel_stack();

        scheduled_time = DEFAULT_SCHEDULE_TIME_MS;
        state = ThreadState::Ready;
        registers.cs = System::Idt::KERNEL_CODE;
        registers.ss = System::Idt::KERNEL_DATA;
        registers.rflags = SET_RFLAGS;

        g_total_tid++;
        thread_id = g_total_tid;
    }    

    void ThreadBlock::init_kernel_stack() {
       Util::IrqRAIGaurd irq_gaurd {};
        
        kernel_pa = Memory::Pmm::alloc(4);
        if (kernel_pa == 0) {
            Util::klog_panic("Pmm::alloc(4) failed in init_kernel_stack() -- out of physical memory\n");
        }
        auto* k_stack = reinterpret_cast<std::uint64_t*>(kernel_pa + Limine::get_hhdm());
        kernel_stack = k_stack;

        auto* top = reinterpret_cast<std::byte*>(k_stack) + THREAD_KERNEL_SIZE;
        rsp0 = reinterpret_cast<std::uint64_t*>(top);

        registers.rsp = reinterpret_cast<std::uint64_t>(rsp0);
    } 

    void ThreadBlock::exit() {
        Util::IrqRAIGaurd irq {};
        Memory::Pmm::free(kernel_pa, 4);
    }

    void ThreadBlock::reset_scheduled_time() {
        scheduled_time = DEFAULT_SCHEDULE_TIME_MS;
    }
        
    void ThreadBlock::update_scheduled_time(std::uint64_t ms) {
        scheduled_time = ms; 
    }
}
