#pragma once

namespace Util::Terminal {
    // Queue the terminal once, after kernel setup and global construction.
    void start();
    [[noreturn]] void run(void*);
}
