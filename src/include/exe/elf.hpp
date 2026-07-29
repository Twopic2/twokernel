#pragma once

#include <cstdint>


/*
    ! ELF Overview

    * The ELF header
      Contains the magic number used to identify the file as an ELF, plus
      info about the architecture it was compiled for, the target OS, and
      other useful info. This is the `e_ident` field.

    * The data blob
      The bulk of the file. One big binary blob containing code, all kinds
      of data, some string tables, and sometimes debugging information.
      All program data lives here.

    * Section headers
      Each header has a name and metadata describing a region of the data
      blob. Names usually begin with a dot (`.`), e.g. `.strtab` for the
      string table. These exist for other software to parse the ELF and
      understand its structure and contents.

    * Program headers
      These are for the program loader (what we're writing). Each header
      has a type telling the loader how to interpret it, plus a range
      within the data blob. These ranges can overlap with — or cover the
      same area as — ranges described by section headers.

 */

/* Credit to Qwinci inside https://github.com/Qwinci/crescent/blob/main/src/exe/elf.hpp */
namespace Exe::Elf {
	inline constexpr std::uint16_t ELFMACHINE = 0x3e;
    inline constexpr std::size_t EI_NIDENT = 16;

    enum class ElfType : std::uint16_t {
        None = 0,
        Rel = 1,
        Exec = 2,
        Dyn = 3 
    };

	enum class ProgramType : std::uint32_t {
	 	Null = 0,
        Load = 1,
        Dynamic = 2,
	};

    struct Elf64_Header {
        unsigned char e_ident[EI_NIDENT];
        ElfType e_type;
        std::uint16_t e_machine;
        std::uint32_t e_version;
        std::uint64_t e_entry;
        std::uint64_t e_phoff;
        std::uint64_t e_shoff;
        std::uint32_t e_flags;
        std::uint16_t e_ehsize;
        std::uint16_t e_phentsize;
        std::uint16_t e_phnum;
        std::uint16_t e_shentsize;
        std::uint16_t e_shnum;
        std::uint16_t e_shstrndx;
    };

	 struct Elf64_Phdr {
        ProgramType p_type;
        std::uint32_t p_flags;
        std::uint64_t p_offset;
        std::uint64_t p_vaddr;
        std::uint64_t p_paddr;
        std::uint64_t p_filesz;
        std::uint64_t p_memsz;
        std::uint64_t p_align;
    };
}
