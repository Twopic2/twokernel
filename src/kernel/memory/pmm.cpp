#include "limine.h"
#include "limine/requests.hpp"
#include "util/kernel_logger.hpp"
#include <cstddef>
#include <cstdint>
#include <memory/pmm.hpp>

namespace Memory::Pmm {
    FreeList* freelist_head = nullptr;

    void* alloc(std::size_t count) {
        FreeList* prev = nullptr;
        FreeList* curr = freelist_head;

        while (curr) {
            if (curr->num_pages >= count) {
                std::uint64_t phys = reinterpret_cast<std::uint64_t>(curr) - get_hhdm();

                if (prev) {
                    prev->next = curr->next;
                } else {
                    freelist_head = curr->next;
                }

                if (curr->num_pages > count) {
                    std::uint64_t phys_offset = phys + count * page_size;
                    std::uint64_t page_offset = curr->num_pages - count;
                    
                    push_list(phys_offset, page_offset);
                }

                return reinterpret_cast<void*>(phys);
            }
            prev = curr;
            curr = curr->next;
        }

        return nullptr;
    }

    void free(void* phys_mem, std::uint64_t count) {
        push_list(reinterpret_cast<std::uint64_t>(phys_mem), count);
    }

    void init_pmm() {
        const auto memmaps = Limine::memmap.response->entries;

        Util::klog("Init PMM\n");
        for (std::size_t i {0}; i < Limine::memmap.response->entry_count; i++) {
            auto* entry = memmaps[i];

            if (entry->type == LIMINE_MEMMAP_USABLE) {
                // entry->base would be starting physical address
                // entry->length: size of memory    
                std::uint64_t entry_page_amount = static_cast<uint64_t>(entry->length / page_size);

                push_list(entry->base, entry_page_amount);
            }
        } 
        Util::klog("Finished mapping physical mem to virtual mem\n");
    }

    void push_list(std::uint64_t phys, std::uint64_t page_amount) {
        FreeList* virtual_mem = reinterpret_cast<FreeList*>(get_hhdm() + phys);  
        
        virtual_mem->num_pages = page_amount;

        virtual_mem->next = freelist_head;
        freelist_head = virtual_mem;        
        
        Util::klog("push_list: phys=0x%llx virt=0x%llx pages=%llu\n",                                                                                                                             
        phys, reinterpret_cast<std::uint64_t>(virtual_mem), page_amount);   
    }
}
