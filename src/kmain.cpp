#include <boot/boot.hpp>

extern "C" void kmain() {
    CxxRuntime::run_global_ctors();

    // Once we finish we halt
    CxxRuntime::hcf();
}
