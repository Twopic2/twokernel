#include "memory/vmm.hpp"
#include "limine/requests.hpp"
#include "memory/pmm.hpp"
#include <cstddef>
#include <cstdint>
#include "arch/x86-64/arch/cpu.hpp"
#include "util/align.hpp"

extern "C" void* _text_start[];   
extern "C" void* _text_end[];
extern "C" void* _rodata_start[];
extern "C" void* _rodata_end[];
extern "C" void*  _data_start[];
extern "C" void* _data_end[];

namespace Memory::Vmm {
    void init() {
//        Util::klog("Init VMM\n");

        kernel_space = new_pagemap();
        fill_kernel_entries(kernel_space);
      
//        Util::klog("Mapping Kernel Space\n");
        const auto* memmap = Limine::memmap.response;

        for (std::size_t i = 0; i < memmap->entry_count; i++) {
            struct limine_memmap_entry* entry = memmap->entries[i];

            if (entry->type == Limine::Memmap::usable||
                entry->type == Limine::Memmap::framebuffer ||
                entry->type == Limine::Memmap::bootloader ||
                entry->type == Limine::Memmap::kernel_and_modules) {

                std::uint64_t v_addr = Limine::get_hhdm() + entry->base;

                map(*kernel_space.pml4, entry->base, v_addr, entry->length,
                    PRESENT | WRITEABLE | NX);
            }
        }
        // Util::klog("Finished mapping Kernel's Virtual address\n");

//        Util::klog("Started limine_executable_address_request Mapping\n");        
        map_limine_kernel_addr(reinterpret_cast<std::uint64_t>(_text_start),
                               reinterpret_cast<std::uint64_t>(_text_end),
                               PRESENT);
        map_limine_kernel_addr(reinterpret_cast<std::uint64_t>(_rodata_start),
                               reinterpret_cast<std::uint64_t>(_rodata_end),
                               PRESENT | NX);
        map_limine_kernel_addr(reinterpret_cast<std::uint64_t>(_data_start),
                               reinterpret_cast<std::uint64_t>(_data_end),
                               PRESENT | WRITEABLE | NX);
       // Util::klog("Finished limine_executable_address_request Mapping\n");

        std::uint64_t efer = 0;
        asm volatile("rdmsr" : "=A"(efer) : "c"(Cpu::MSREFER));
        efer |= (1 << 11);
        asm volatile("wrmsr" :: "c"(Cpu::MSREFER), "A"(efer));

        load_cr3(kernel_space.pml4_pa);
        // Util::klog("VMM loaded CR3=0x%llx\n", kernel_space.pml4_pa);

//        Util::klog("Finished setting up VMM\n");

        for (std::size_t i = 0; i < memmap->entry_count; i++) {
            auto* entry = memmap->entries[i];

            if (entry->length < PAGE_SIZE) {
                continue;
            }

            auto _page_amount = entry->length / PAGE_SIZE;
            Pmm::reclaim_bootloader(entry->type, entry->base, _page_amount);
        }
    }

    void map_limine_kernel_addr(std::uint64_t start, std::uint64_t end, std::uint64_t flags) {
        std::size_t length = end - start;
        std::uint64_t pa = start - Limine::kernel_address.response->virtual_base + Limine::kernel_address.response->physical_base;
        
        map(*kernel_space.pml4, pa, start, length, flags);
    }

    void map(struct PageMap& pagemap, std::uint64_t pa, std::uint64_t va, 
        std::size_t length, std::uint64_t flags) {
        for (std::size_t off = 0; off < length; off += PAGE_SIZE) {
            std::uint64_t cur_va = Util::Align::align_down(va + off, PAGE_SIZE);
            std::uint64_t cur_pa = Util::Align::align_down(pa + off, PAGE_SIZE);

            // Credit to Mintsuki
            std::size_t pml4_entry = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 39)) >> 39;
            std::size_t pdpt_entry = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 30)) >> 30;
            std::size_t pd_entry   = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 21)) >> 21;
            std::size_t pt_entry   = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 12)) >> 12;

            PageMap* pdpt = get_next_level(pagemap, pml4_entry, flags, true);
            PageMap* pd   = get_next_level(*pdpt,   pdpt_entry, flags, true);
            PageMap* pt   = get_next_level(*pd,     pd_entry,   flags, true);

            pt->entries[pt_entry] = cur_pa | flags;
        }
    }

    void unmap(struct PageMap& pagemap, std::uint64_t va, std::size_t length) {
        for (std::size_t i = 0; i < length; i += PAGE_SIZE) {
            std::uint64_t cur_va = Util::Align::align_down(va + i, PAGE_SIZE);

            std::size_t pml4_entry = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 39)) >> 39;
            std::size_t pdpt_entry = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 30)) >> 30;
            std::size_t pd_entry   = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 21)) >> 21;
            std::size_t pt_entry   = (cur_va & (static_cast<std::uint64_t>(0x1ff) << 12)) >> 12;

            PageMap* pdpt = get_next_level(pagemap, pml4_entry, 0, false);
            PageMap* pd   = get_next_level(*pdpt,   pdpt_entry, 0, false);
            PageMap* pt   = get_next_level(*pd,     pd_entry,   0, false);

            pt->entries[pt_entry] = 0;
        }   
    }

    PageMap* get_next_level(struct PageMap& entry, std::size_t cur_level, std::uint64_t flags, bool allocate) {
        std::uint64_t _entry = entry.entries[cur_level];

        if (_entry & PRESENT) {
            std::uint64_t next_pa = _entry & PADDR_MASK;
            std::uint64_t next_va = next_pa + Limine::get_hhdm();
            
            PageMap* next_level = reinterpret_cast<PageMap*>(next_va);
            return next_level;        
        }

        if (allocate) {
            std::uint64_t new_pa = Pmm::alloc(1);
            PageMap* next_level = reinterpret_cast<PageMap*>(new_pa + Limine::get_hhdm());

            LibC::memset(next_level->entries.data(), 0, PAGE_SIZE);
            asm volatile("sfence" : : : "memory"); // Ensure zeroing is visible before entry is set.
            
            if (flags & USER) {
                entry.entries[cur_level] = new_pa | PRESENT | WRITEABLE | USER;
            }
            entry.entries[cur_level] = new_pa | PRESENT | WRITEABLE ;

            return next_level;
        } 
        
        return nullptr;
    }

    std::uint64_t va_to_pa(struct VAddressSpace& space, std::uint64_t va) {
        std::size_t pml4_entry = (va & (static_cast<std::uint64_t>(0x1ff) << 39)) >> 39;
        std::size_t pdpt_entry = (va & (static_cast<std::uint64_t>(0x1ff) << 30)) >> 30;
        std::size_t pd_entry   = (va & (static_cast<std::uint64_t>(0x1ff) << 21)) >> 21;
        std::size_t pt_entry   = (va & (static_cast<std::uint64_t>(0x1ff) << 12)) >> 12;

        PageMap* pdpt = get_next_level(*space.pml4, pml4_entry, 0, false);
        PageMap* pd   = get_next_level(*pdpt,      pdpt_entry, 0, false);
        PageMap* pt   = get_next_level(*pd,        pd_entry,   0, false);

        return (pt->entries[pt_entry] & PADDR_MASK) | (va & OFFMASK);
    }

    void fill_kernel_entries(struct VAddressSpace& vaddr) {
        // Util::klog("kernel_space upper half vaddress init\n");        
        for (std::size_t i = 256; i < 512; i++) {
            std::uint64_t pdpt_pa = Pmm::alloc(1);
            auto* pdpt = reinterpret_cast<PageMap*>(pdpt_pa + Limine::get_hhdm());
            LibC::memset(pdpt->entries.data(), 0, sizeof(pdpt->entries));
            asm volatile("sfence" : : : "memory"); // Ensure zeroing is visible before entry is set.
            vaddr.pml4->entries[i] = pdpt_pa | PRESENT | WRITEABLE;
        }
        // Util::klog("finished kernel_space upper half vaddress init\n");
    }

    /// for adding a new vaddr proc
    void process_fill_kernel_entries(struct VAddressSpace& vaddr) {
        for (std::size_t i = 256; i < 512; i++) {
            vaddr.pml4->entries[i] = kernel_space.pml4->entries[i];
        }
    }
}
