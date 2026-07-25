#include <tests/ktest.hpp>
#include <memory/vmm.hpp>
#include <memory/pmm.hpp>
#include <limine/requests.hpp>

namespace Tests {
    TEST_CASE("Vmm::init built the kernel address space", "[vmm]") {
        REQUIRE(Memory::Vmm::kernel_space.pml4 != nullptr);
        CHECK(Memory::Vmm::kernel_space.pml4_pa != 0);
    }

    TEST_CASE("all upper half PML4 entries (256-511) are present", "[vmm]") {
        bool upper_half_ok = true;
        for (std::size_t i = 256; i < 512; i++) {
            if (!(Memory::Vmm::kernel_space.pml4->entries[i] & Memory::Vmm::PRESENT)) {
                upper_half_ok = false;
                break;
            }
        }
        CHECK(upper_half_ok);
    }

    TEST_CASE("va_to_pa recovers the physical address behind an HHDM mapping", "[vmm]") {
        std::uint64_t phys = Memory::Pmm::alloc(1);
        REQUIRE(phys != 0);

        std::uint64_t hhdm_va  = phys + Limine::get_hhdm();
        std::uint64_t recovered = Memory::Vmm::va_to_pa(Memory::Vmm::kernel_space, hhdm_va);
        CHECK(recovered == phys);

        Memory::Pmm::free(phys, 1);
    }

    TEST_CASE("HHDM virtual addresses are writable", "[vmm]") {
        std::uint64_t phys = Memory::Pmm::alloc(1);
        REQUIRE(phys != 0);

        auto* vptr = reinterpret_cast<std::uint64_t*>(phys + Limine::get_hhdm());
        *vptr = 0xDEADBEEFCAFEBABEull;
        CHECK(*vptr == 0xDEADBEEFCAFEBABEull);

        Memory::Pmm::free(phys, 1);
    }
}
