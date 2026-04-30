#include <tests/ktest.hpp>
#include <memory/vmm.hpp>
#include <memory/pmm.hpp>
#include <limine/requests.hpp>

namespace Tests {
    void run_vmm_tests() {
        Util::klog("--- VMM Tests ---\n");
        KTest::reset();

        // kernel_space was set up by init()
        KTEST_ASSERT(Memory::Vmm::kernel_space.pml4    != nullptr, "kernel_space pml4 is set");
        KTEST_ASSERT(Memory::Vmm::kernel_space.pml4_pa != 0,       "kernel_space pml4_pa is non-zero");

        // upper half PML4 entries 256-511 should all be present
        bool upper_half_ok = true;
        for (std::size_t i = 256; i < 512; i++) {
            if (!(Memory::Vmm::kernel_space.pml4->entries[i] & Memory::Vmm::PRESENT)) {
                upper_half_ok = false;
                break;
            }
        }
        KTEST_ASSERT(upper_half_ok, "all upper half PML4 entries (256-511) are present");

        // HHDM: va_to_pa on a known physical page should recover the physical address
        std::uintptr_t phys = Memory::Pmm::alloc(1);
        KTEST_ASSERT(phys != 0, "PMM alloc for va_to_pa test succeeded");

        std::uintptr_t hhdm_va = phys + Limine::get_hhdm();
        std::uintptr_t recovered = Memory::Vmm::va_to_pa(Memory::Vmm::kernel_space, hhdm_va);
        KTEST_ASSERT(recovered == phys, "va_to_pa recovers correct physical address via HHDM");

        Memory::Pmm::free(phys, 1);

        // HHDM write/read through virtual address
        std::uintptr_t phys2 = Memory::Pmm::alloc(1);
        auto* vptr = reinterpret_cast<std::uint64_t*>(phys2 + Limine::get_hhdm());
        *vptr = 0xDEADBEEFCAFEBABEull;
        KTEST_ASSERT(*vptr == 0xDEADBEEFCAFEBABEull, "HHDM virtual address is writable");
        Memory::Pmm::free(phys2, 1);

        KTest::summary("VMM");
    }
}
