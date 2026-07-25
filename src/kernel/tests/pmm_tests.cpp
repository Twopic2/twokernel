#include <cstdint>
#include <tests/ktest.hpp>
#include <memory/pmm.hpp>
#include <memory/vmm.hpp>
#include <limine/requests.hpp>

namespace Tests {
    TEST_CASE("Pmm::alloc returns distinct non-zero pages", "[pmm]") {
        std::uint64_t pa1 = Memory::Pmm::alloc(1);
        REQUIRE(pa1 != 0);

        std::uint64_t pa2 = Memory::Pmm::alloc(1);
        REQUIRE(pa2 != 0);
        CHECK(pa2 != pa1);

        Memory::Pmm::free(pa1, 1);
        Memory::Pmm::free(pa2, 1);
    }

    TEST_CASE("allocated pages are writable through the HHDM", "[pmm]") {
        std::uint64_t pa = Memory::Pmm::alloc(1);
        REQUIRE(pa != 0);

        auto* ptr = reinterpret_cast<std::uint64_t*>(pa + Limine::get_hhdm());
        *ptr = 0xCAFEBABEull;
        CHECK(*ptr == 0xCAFEBABEull);

        Memory::Pmm::free(pa, 1);
    }

    TEST_CASE("multi-page allocation succeeds", "[pmm]") {
        std::uint64_t pa = Memory::Pmm::alloc(4);
        REQUIRE(pa != 0);
        Memory::Pmm::free(pa, 4);
    }

    TEST_CASE("freed pages go back to the pool", "[pmm]") {
        std::uint64_t pa = Memory::Pmm::alloc(1);
        REQUIRE(pa != 0);
        Memory::Pmm::free(pa, 1);

        std::uint64_t again = Memory::Pmm::alloc(1);
        CHECK(again != 0);
        Memory::Pmm::free(again, 1);
    }

    TEST_CASE("Pmm::alloc returns page-aligned addresses", "[pmm]") {
        std::uint64_t pa = Memory::Pmm::alloc(1);
        REQUIRE(pa != 0);
        CHECK((pa & 0xFFF) == 0);
        Memory::Pmm::free(pa, 1);
    }
}
