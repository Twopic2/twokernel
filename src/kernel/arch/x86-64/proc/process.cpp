#include "arch/x86-64/proc/process.hpp"
#include "arch/x86-64/arch/arch_irq.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "arch/x86-64/proc/thread.hpp"
#include "limine/requests.hpp"
#include "memory/vmm.hpp"
#include "util/kernel_logger.hpp"
#include <cstdint>

using namespace x86::Proc::Scheduler;
using namespace Memory::Vmm;
using namespace Memory::AddrSpace;

namespace x86::Proc::Process {
    ProcessBlock::ProcessBlock(std::string_view name, bool user) :
    is_user(user), is_dead(false), m_name(name) {
        Util::IrqRAIGaurd irq {};
        
        process_fill_kernel_entries(address_space.page_dir);

        if (user) {
            process_fill_user_entires(address_space.page_dir);

        }

        status = ProcStats::Ready;
        total_pid++;
        pid = total_pid;
    }

    ProcessBlock::ProcessBlock() {
        Util::IrqRAIGaurd irq {};
        process_fill_kernel_entries(address_space.page_dir);
        status = ProcStats::Ready;
        total_pid++;
        pid = total_pid;
    }

    void ProcessBlock::add_thread(Thread::ThreadBlock* thread) {
        Util::IrqRAIGaurd irq {};
        thread->m_process = this;
        threads.push_tail(thread);
        thread_size++;
        Thread::g_total_tid++;
        thread->thread_id = Thread::g_total_tid;
    }

    void ProcessBlock::exit(int stats) {
        Util::IrqRAIGaurd irq {};
        if (stats == -1) {
            if (parent) {
                status = ProcStats::Zombie;
                is_zombie = true;
                exit_status = stats;

                const auto sucess = g_scheduler.remove_dead_process_threads();
                if (!sucess) {
                    Util::klog_panic("Something went wrong inside process.cpp");
                }

                remove_all_threads();
                return;
            } else {
                status = ProcStats::Dead;
                is_zombie = true;

                const auto sucess = g_scheduler.remove_dead_process_threads();
                if (!sucess) {
                    Util::klog_panic("Something went wrong inside process");
                }

                remove_all_threads();
            }     
           
        } 
    }
}
