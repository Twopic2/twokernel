#pragma once

#include <cstdint>
#include <limine.h>
#include <std/cstdint>

namespace Limine {
    extern volatile limine_framebuffer_request framebuffer_request;
    void base_revision_check();

    namespace Memmap {
        inline constexpr std::uint64_t usable             = LIMINE_MEMMAP_USABLE;
        inline constexpr std::uint64_t reserved           = LIMINE_MEMMAP_RESERVED;                                                     
        inline constexpr std::uint64_t acpi_reclaimable   = LIMINE_MEMMAP_ACPI_RECLAIMABLE;                                                                                                                                                                                                                  
        inline constexpr std::uint64_t acpi_nvs           = LIMINE_MEMMAP_ACPI_NVS;                                                                                                                                                                                                                          
        inline constexpr std::uint64_t bad_memory         = LIMINE_MEMMAP_BAD_MEMORY;                                                                                                                                                                                                                        
        inline constexpr std::uint64_t bootloader         = LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;                                                                                                                                                                                                            
        inline constexpr std::uint64_t kernel_and_modules = LIMINE_MEMMAP_EXECUTABLE_AND_MODULES;                                                                                                                                                                                                            
        inline constexpr std::uint64_t framebuffer        = LIMINE_MEMMAP_FRAMEBUFFER;
        inline constexpr std::uint64_t reserved_mapped    = LIMINE_MEMMAP_RESERVED_MAPPED;    
    } 
    
    [[gnu::used, gnu::section(".limine_requests")]]
    inline volatile limine_paging_mode_request paging_mode
    {
        .id = LIMINE_PAGING_MODE_REQUEST_ID,
        .revision = 0,
        .response = nullptr,
        .mode = LIMINE_PAGING_MODE_X86_64_DEFAULT,
        .max_mode = LIMINE_PAGING_MODE_X86_64_DEFAULT,
        .min_mode = LIMINE_PAGING_MODE_X86_64_DEFAULT
    };

    [[gnu::used, gnu::section(".limine_requests")]]
    inline volatile limine_memmap_request memmap
    {
        .id = LIMINE_MEMMAP_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };

    [[gnu::used, gnu::section(".limine_requests")]]
    inline volatile limine_executable_address_request kernel_address
    {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };

    [[gnu::used, gnu::section(".limine_requests")]]
    inline volatile limine_hhdm_request hhdm
    {
        .id = LIMINE_HHDM_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };

     // write into or read from a physical address
    inline std::uint64_t get_hhdm() {
        return Limine::hhdm.response->offset; 
    }
} 
