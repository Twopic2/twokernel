#pragma once

#include "arch/x86-64/arch/event.hpp"
#include <cstddef>

namespace Drivers::Keyboard {
    struct KeySequence {
        char bytes[8] {};
        std::size_t length = 0;
    };

    // US layout, ASCII control characters, and VT navigation sequences.
    KeySequence translate_key(Arch::KeyEvent event);
}
