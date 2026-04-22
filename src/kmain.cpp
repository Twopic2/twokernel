#include "memory/pmm.hpp"
#include <boot/boot.hpp>
#include <limine/requests.hpp>
#include <util/kernel_logger.hpp>
#include <drivers/fbtty.hpp>
#include <drivers/framebuffer.hpp>
#include <arch/x86-64/arch.hpp>

extern "C" {
#include <flanterm.h>
#include <flanterm_backends/fb.h>
}

extern "C" void kmain() {
    // Ensures the proper base revision (see spec).
    Limine::base_revision_check();

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

    Memory::Pmm::init_pmm();

    // Once we finish we halt
    CxxRuntime::hcf();
}
