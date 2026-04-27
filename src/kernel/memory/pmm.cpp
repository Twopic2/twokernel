#include "limine.h"
#include "limine/requests.hpp"
#include "memory/vmm.hpp"
#include "util/kernel_logger.hpp"
#include <cstddef>
#include <cstdint>
#include <memory/pmm.hpp>

namespace Memory::Pmm {
    std::uintptr_t alloc(std::size_t count) {
        FreeList* prev = nullptr;
        FreeList* curr = freelist_head;

        while (curr) {
            if (curr->num_pages >= count) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    freelist_head = curr->next;
                }

                if (curr->num_pages > count) {
                    std::uint64_t phys_offset = curr->pa + count * Vmm::page_size;
                    std::uint64_t page_offset = curr->num_pages - count;
                    
                    push_list(phys_offset, page_offset);
                }

                return curr->pa;
            }

            prev = curr;
            curr = curr->next;
        }

        return 0;
    }

    void free(std::uintptr_t phys_mem, std::uint64_t count) {
        push_list(phys_mem, count);
    }

    void init_pmm() {
        const auto memmaps = Limine::memmap.response->entries;

        Util::klog("Init PMM\n");
        for (std::size_t i {0}; i < Limine::memmap.response->entry_count; i++) {
            auto* entry = memmaps[i];

            if (entry->type == Limine::Memmap::usable) {
                // entry->base would be starting physical address
                // entry->length: size of memory    
                std::uint64_t entry_page_amount = static_cast<uint64_t>(entry->length / Vmm::page_size);         
                
                push_list(entry->base, entry_page_amount);
            }
        } 
        Util::klog("Finished mapping physical mem to virtual mem\n");
    }

    void push_list(std::uint64_t phys, std::uint64_t page_amount) {
        FreeList* node = reinterpret_cast<FreeList*>(phys + Limine::get_hhdm());
        
        node->pa = phys;
        node->num_pages = page_amount;

        node->next = freelist_head;
        freelist_head = node;        
    }

    void free_limine_bootloader(std::uint64_t memap_type, std::uintptr_t phys_mem, std::uintptr_t page_amount) {
        Util::klog("Reclamining Bootloader's Memory\n");
        
        if (memap_type == Limine::Memmap::bootloader) {
            push_list(phys_mem, page_amount);
        }

        Util::klog("Finished Reclaiming Bootloader's memory\n");
    }
}
