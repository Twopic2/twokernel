#include <boot/boot.hpp>

namespace CxxRuntime {
    [[noreturn]] void hcf() {
        for (;;) {
            asm volatile("hlt");
        }
    }

    extern "C" {
        int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
        void __cxa_pure_virtual() { hcf(); }
        void *__dso_handle;
    }

    extern "C" void (*__init_array[])();
    extern "C" void (*__init_array_end[])();

    void run_global_ctors() {
        for (std::size_t i = 0; &__init_array[i] != __init_array_end; i++) {
            __init_array[i]();
        }
    }
}
