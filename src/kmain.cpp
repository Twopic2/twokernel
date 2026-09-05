#include "arch/x86-64/arch/syscall.hpp"
#include "arch/x86-64/dev/timer.hpp"
#include "arch/x86-64/dev/keyboard.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "tests/ktest.hpp"
#include "memory/pmm.hpp"
#include "memory/vmm.hpp"
#include <boot/boot.hpp>
#include <limine/requests.hpp>
#include <util/kernel_logger.hpp>
#include <drivers/fbtty.hpp>
#include <drivers/framebuffer.hpp>
#include <arch/x86-64/arch/cpu.hpp>
#include <arch/x86-64/arch.hpp>
#include "util/ansi.hpp"

extern "C" {
#include <flanterm.h>
#include <flanterm_backends/fb.h>
}

extern "C" void kmain() {
    // Ensures the proper base revision (see spec).
    Limine::base_revision_check();

    Memory::Pmm::init_pmm();
    Memory::Vmm::init();

    CxxRuntime::run_global_ctors();

    if (Limine::framebuffer_request.response == NULL
     || Limine::framebuffer_request.response->framebuffer_count < 1) {
        CxxRuntime::hcf();
    }

    limine_framebuffer *framebuffer = Limine::framebuffer_request.response->framebuffers[0];
    
    Drivers::FbTty fbtty(framebuffer);
    fbtty.fb_init();
    Drivers::g_tty = &fbtty;

    Util::klog("Started Kernel\n");

    auto first_init = x86::inital_init();
    
    if (first_init) {
        x86::exception_load();
    }

   // KTest::run("[pmm]");
    //KTest::run("[vmm]");

    KTest::run("[scheduler]");

    x86::Dev::Timer::timer_init();

    x86::Dev::Keyboard::keyboard_init();

    Syscalls::init_syscalls();
    
    Cpu::init_cpu();
    Cpu::init_syscall();


//    KTest::run("[deque]");

    //KTest::run("[keyboard]");
    //KTest::run("[irq]");
    //KTest::run("[ringbuffer]");

    // KTest::run("[elf]"); 

    // Once we finish we halt
}
