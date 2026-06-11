#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include "limine/requests.hpp"
#include "memory/vmm.hpp"
#include <cstdint>

namespace x86::Proc::Process {
    ProcessBlock::ProcessBlock(const char* name, FuncPtr entry) :
    m_entry(entry), m_name(name) {
       // Util::IrqGaurd irq {};
        vaddr = Memory::Vmm::new_pagemap();
        Memory::Vmm::process_fill_kernel_entries(vaddr);
    }

    ProcessBlock::ProcessBlock() {
    //    Util::IrqGaurd irq {};
        vaddr = Memory::Vmm::new_pagemap();
        Memory::Vmm::process_fill_kernel_entries(vaddr);
        status = ProcStats::Ready;
    }

    void ProcessBlock::add_thread(Thread::ThreadBlock* thread) {
        //Util::IrqGaurd irq {};
        thread->m_process = this;
        threads.push_tail(thread);
        thread_size++;

        Thread::total_tid++;
        thread->thread_id = Thread::total_tid;
    }

    void ProcessBlock::exit() {
        //Util::IrqGaurd irq {};
        status = ProcStats::Dead;
    }
}
