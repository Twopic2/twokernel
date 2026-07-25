#pragma once

#include <cstdint>

/* Credit to Qwinci for some ideas */

namespace Exe::Elf {
    enum class ElfType : std::uint16_t {
        None = 0,
        Rel = 1,
        Exec = 2,
        Dyn = 3 
    };
    
    struct Elf64_Ehdr {

    };
}
