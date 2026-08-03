#include"exe/elf_loader.hpp"
#include "exe/elf.hpp"

namespace Exe::Elf {
    bool elf_check_file(const Elf64_Header& elf_hdr) {
        if (elf_hdr.e_ident[EI_MAG0] != ELFMAG0) {
            return false;
        }

        if (elf_hdr.e_ident[EI_MAG1] != ELFMAG1) {
            return false;
        }
        
        if (elf_hdr.e_ident[EI_MAG2] != ELFMAG2) {
            return false;
        }
        
        if (elf_hdr.e_ident[EI_MAG3] != ELFMAG3) {
            return false;
        }
        
        return true;
    }

    bool elf_check_supported(const Elf64_Header& elf_hdr) {
        if (elf_hdr.e_ident[EI_CLASS] != ELFCLASS64) {
            return false;
        }

        return true;
    } 
}