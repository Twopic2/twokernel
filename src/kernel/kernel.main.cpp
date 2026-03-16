
extern "C" char __init_array[];
extern "C" char __init_array_end[];

void kernel_main() {
    for (auto it = (void(*)())__init_array; it < (void(*)())__init_array_end; it++) {
        (*it)();
    }
}