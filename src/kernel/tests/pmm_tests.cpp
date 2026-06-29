#include <cstdint>
#include <tests/ktest.hpp>
#include <memory/pmm.hpp>
#include <memory/vmm.hpp>
#include <limine/requests.hpp>

namespace Tests {
    void run_pmm_tests() {
        Util::klog("--- PMM Tests ---\n");
        KTest::reset();

        // alloc returns a non-zero physical address
        std::uint64_t pa1 = Memory::Pmm::alloc(1);
        KTEST_ASSERT(pa1 != 0, "alloc(1) returns non-zero");

        // alloc returns a second different address
        std::uint64_t pa2 = Memory::Pmm::alloc(1);
        KTEST_ASSERT(pa2 != 0,   "alloc(1) second call returns non-zero");
        KTEST_ASSERT(pa2 != pa1, "alloc(1) returns unique addresses");

        // allocated address is accessible via HHDM and writable
        auto* ptr = reinterpret_cast<std::uint64_t*>(pa1 + Limine::get_hhdm());
        *ptr = 0xCAFEBABEull;
        KTEST_ASSERT(*ptr == 0xCAFEBABEull, "allocated page is writable via HHDM");

        // multi-page alloc
        std::uint64_t pa3 = Memory::Pmm::alloc(4);
        KTEST_ASSERT(pa3 != 0, "alloc(4) returns non-zero");

        // free returns pages to pool — next alloc should succeed
        Memory::Pmm::free(pa1, 1);
        Memory::Pmm::free(pa2, 1);
        Memory::Pmm::free(pa3, 4);

        std::uint64_t pa4 = Memory::Pmm::alloc(1);
        KTEST_ASSERT(pa4 != 0, "alloc after free returns non-zero");
        Memory::Pmm::free(pa4, 1);

        // pages are 4KB aligned
        std::uint64_t pa5 = Memory::Pmm::alloc(1);
        KTEST_ASSERT((pa5 & 0xFFF) == 0, "alloc returns page-aligned address");
        Memory::Pmm::free(pa5, 1);

        KTest::summary("PMM");
    }
}
