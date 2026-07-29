#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>

/* 
    ! Elf Object 

    * Loaded into memory
    * - `.text`   — the actual machine code (program instructions)
    * - `.data`   — initialized globals: tables, variables, etc.
    * - `.rodata` — read-only data, mostly string literals
    *                (compiler-dependent; similar extra sections may exist)
    * - `.bss`    — UNinitialized globals. Takes no space on disk —
    *                the loader creates it and zero-fills it at load time.

*/

/* Credit to @Qwinci 
https://github.com/Qwinci/crescent/blob/main/src/exe/elf_loader.hpp */
namespace Exe::ElfLoader {
    struct LoadedElf {
        void (*entry)(void*);
        std::size_t base;
        std::size_t phdrs_addr;
        std::uint16_t phdr_count;
        std::uint16_t phdr_size;
    };

    enum class ElfLoadError {
        Invalid,
        NoMemory
    };

    std::expected<LoadedElf, ElfLoadError> elf_load();
}
