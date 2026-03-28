#include <boot/boot.hpp>
#include <limine/requests.hpp>
#include <include/util/kernel_logger.hpp>
#include <include/drivers/fbtty.hpp>
#include <include/drivers/framebuffer.hpp>

#include <cstdint>

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

    int len = Util::printk("Hello world kernel\n");
    fbtty.write_terminal(Util::char_buff, len);

    // Once we finish we halt
    CxxRuntime::hcf();
}
