#include <boot/boot.hpp>
#include <limine/requests.hpp>

#include <std/cstdint>

extern "C" void kmain() {
    // Ensures the proper base revision (see spec).
    Limine::base_revision_check();

    CxxRuntime::run_global_ctors();

    if (Limine::framebuffer_request.response == NULL
     || Limine::framebuffer_request.response->framebuffer_count < 1) {
        CxxRuntime::hcf();
    }

    // Once we finish we halt
    CxxRuntime::hcf();
}
