#include <boot/boot.hpp>
#include <limine/requests.hpp>
#include <include/drivers/tty.hpp>
#include <include/util/kernel_logger.hpp>

#include <std/cstdint>

extern "C" void kmain() {
    // Ensures the proper base revision (see spec).
    Limine::base_revision_check();

    CxxRuntime::run_global_ctors();

    if (Limine::framebuffer_request.response == NULL
     || Limine::framebuffer_request.response->framebuffer_count < 1) {
        CxxRuntime::hcf();
    }

    limine_framebuffer *framebuffer = Limine::framebuffer_request.response->framebuffers[0];

    Drivers::Tty tty(framebuffer);

    Util::printk("Hello world kernel\n");
    tty.write_terminal(Util::char_buff.data());

    // Once we finish we halt
    CxxRuntime::hcf();
}
