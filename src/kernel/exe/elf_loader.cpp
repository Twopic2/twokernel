#include"exe/elf_loader.hpp"
#include "exe/elf.hpp"
#include "libc/string.hpp"
#include "memory/pmm.hpp"
#include "memory/vmm.hpp"
#include "util/align.hpp"
#include <cstddef>
#include <cstdint>

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

        if (elf_hdr.e_ident[EI_DATA] != ELFDATA2LSB) {
            return false;
        }
        
        if (elf_hdr.e_machine != ELFMACHINE) {
            return false;
        }

        if (elf_hdr.e_type != ElfType::Exec && elf_hdr.e_type != ElfType::Rel) {
            return false;
        }

        if (elf_hdr.e_phnum >= 512) {
            return false;   
        }

        if (elf_hdr.e_phentsize >= 512) {
            return false;
        }

        if (elf_hdr.e_phoff >= UINT32_MAX) {
            return false;
        }        

        return true;
    } 
    
    // TODO make sure to add Process and VFS
    std::expected<LoadedElf, ElfLoadError> elf_load(void* data, std::size_t size, Memory::Vmm::VAddressSpace& space) {
        if (size < sizeof(Elf64_Header)) {
            return std::unexpected(ElfLoadError::Invalid);
        }

        const auto ehdr = reinterpret_cast<Elf64_Header*>(data); 

        if (!elf_check_file(*ehdr)) {
            return std::unexpected(ElfLoadError::Invalid);
        }


        if (!elf_check_supported(*ehdr)) {
            return std::unexpected(ElfLoadError::Invalid);
        }

        std::uint64_t image_base {};
        bool found_base = false;
        auto* bytes = reinterpret_cast<std::uint8_t*>(data);

        for (std::size_t i = 0; i < ehdr->e_phnum; i++) {
            auto phdr = reinterpret_cast<Elf64_Phdr*>(ehdr->e_phoff + i * ehdr->e_phentsize);
            
            /* 
                ! For statically linked ELF's, the most important segment type is the PT_LOAD type. 
                ! These denote sections in the ELF file that must be loaded into memory.
             */
            if (phdr->p_type != ProgramType::Load) {
                continue;
            }


            std::uint64_t flags = Memory::Vmm::PRESENT | Memory::Vmm::USER;
            if (phdr->p_flags & PF_W) {
                flags |= Memory::Vmm::WRITEABLE;
            }

            if (!(phdr->p_flags & PF_X)) {
                flags |= Memory::Vmm::NX;
            }

            const std::uint64_t page_offset = phdr->p_vaddr % Memory::Vmm::PAGE_SIZE;
            const std::uint64_t aligned_base = phdr->p_vaddr - page_offset;
            const std::uint64_t aligned_size = Util::Align::align_up(phdr->p_memsz + page_offset, Memory::Vmm::PAGE_SIZE);
            const std::uint64_t pages = aligned_size / Memory::Vmm::PAGE_SIZE;

            auto pa = Memory::Pmm::alloc(pages);    

            // TODO: Add a proper mmap() syscall
            Memory::Vmm::map(*space.pml4, pa, aligned_base, aligned_size, flags);
            auto* dst = reinterpret_cast<std::uint8_t*>(pa + Limine::get_hhdm());
            
            if (phdr->p_memsz > phdr->p_filesz) {
                LibC::memset(dst, 0, aligned_size);
                LibC::memcpy(dst + page_offset, bytes + phdr->p_offset, phdr->p_filesz);
            }
            
            if (!found_base) {
                image_base = aligned_base;
            }
        }

        return LoadedElf {
            .entry = ehdr->e_entry,
            .base = image_base,
            .phdrs_addr = 0,
            .phdr_count = ehdr->e_phnum,
            .phdr_size = ehdr->e_phentsize,
        };
    }
}
