#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include "arch/x86-64/proc/process.hpp"
#include "elf.hpp"
#include "memory/vmm.hpp"

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
namespace Exe::Elf {
    struct LoadedElf {
        std::uint64_t entry;
        std::size_t base;
        std::size_t end;
        std::size_t phdrs_addr;
        std::uint16_t phdr_count;
        std::uint16_t phdr_size;
    };

    enum class ElfLoadError {
        Invalid,
        NoMemory
    };

    bool elf_check_file(const Elf64_Header& elf_hdr);
    bool elf_check_supported(const Elf64_Header& elf_hdr);
    
    std::expected<LoadedElf, ElfLoadError> elf_load(const void* data, std::size_t size, x86::Proc::Process::ProcessBlock* process);
}
