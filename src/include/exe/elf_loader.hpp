#pragma once

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

namespace Exe::ElfLoader {

}
