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

    namespace {
        constexpr std::uint64_t PAGE = Memory::Vmm::PAGE_SIZE;

        // Every page the allocator hands out is reachable through the HHDM,
        // so writing there is how a test proves two allocations are really
        // distinct physical memory and not the same page twice.
        std::uint64_t* page_at(std::uint64_t pa, std::uint64_t index = 0) {
            return reinterpret_cast<std::uint64_t*>(pa + index * PAGE + Limine::get_hhdm());
        }

        bool ranges_overlap(std::uint64_t a, std::uint64_t a_pages,
                            std::uint64_t b, std::uint64_t b_pages) {
            return a < b + b_pages * PAGE && b < a + a_pages * PAGE;
        }

        // The freelist is a LIFO stack of nodes that live inside the regions
        // they describe, so free(pa, n) always leaves {pa, n} at the head.
        // Priming it that way makes the next alloc's result exact instead of
        // "whatever earlier tests left lying around" -- these cases run
        // against the one real global freelist, so nothing else is knowable.
        std::uint64_t prime_head(std::uint64_t pages) {
            std::uint64_t pa = Memory::Pmm::alloc(pages);
            if (pa != 0) {
                Memory::Pmm::free(pa, pages);
            }
            return pa;
        }
    }

    TEST_CASE("two live allocations are disjoint physical memory", "[pmm]") {
        std::uint64_t p = Memory::Pmm::alloc(1);
        std::uint64_t q = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        REQUIRE(q != 0);

        CHECK(p != q);
        CHECK_FALSE(ranges_overlap(p, 1, q, 1));

        // Aliased pages would show up as the second write landing on the
        // first -- the pointer comparison above can't catch that on its own.
        *page_at(p) = 0x1111111111111111ull;
        *page_at(q) = 0x2222222222222222ull;
        CHECK(*page_at(p) == 0x1111111111111111ull);
        CHECK(*page_at(q) == 0x2222222222222222ull);

        Memory::Pmm::free(q, 1);
        Memory::Pmm::free(p, 1);
    }

    TEST_CASE("two live multi-page allocations are disjoint", "[pmm]") {
        std::uint64_t p = Memory::Pmm::alloc(4);
        std::uint64_t q = Memory::Pmm::alloc(4);
        REQUIRE(p != 0);
        REQUIRE(q != 0);

        CHECK_FALSE(ranges_overlap(p, 4, q, 4));

        // Touch the first and last page of each run: a 4-page allocation has
        // to own all four, not just the one its address points at.
        *page_at(p, 0) = 0xAAAA0000ull;
        *page_at(p, 3) = 0xAAAA0003ull;
        *page_at(q, 0) = 0xBBBB0000ull;
        *page_at(q, 3) = 0xBBBB0003ull;

        CHECK(*page_at(p, 0) == 0xAAAA0000ull);
        CHECK(*page_at(p, 3) == 0xAAAA0003ull);
        CHECK(*page_at(q, 0) == 0xBBBB0000ull);
        CHECK(*page_at(q, 3) == 0xBBBB0003ull);

        Memory::Pmm::free(q, 4);
        Memory::Pmm::free(p, 4);
    }

    // alloc, alloc, free, free -- both pages come back, and because the
    // freelist is LIFO they come back in reverse order of the frees.
    TEST_CASE("freeing two allocations returns both pages to the pool", "[pmm]") {
        std::uint64_t p = Memory::Pmm::alloc(1);
        std::uint64_t q = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        REQUIRE(q != 0);

        Memory::Pmm::free(p, 1);        // head: {p, 1}
        Memory::Pmm::free(q, 1);        // head: {q, 1} -> {p, 1}

        std::uint64_t first  = Memory::Pmm::alloc(1);
        std::uint64_t second = Memory::Pmm::alloc(1);
        CHECK(first == q);
        CHECK(second == p);

        Memory::Pmm::free(second, 1);
        Memory::Pmm::free(first, 1);
    }

    // alloc, free, alloc, free -- the single most useful invariant here: a
    // page freed a moment ago is the very next page handed out.
    TEST_CASE("an immediate free hands the same page back", "[pmm]") {
        std::uint64_t p = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        Memory::Pmm::free(p, 1);

        std::uint64_t q = Memory::Pmm::alloc(1);
        CHECK(q == p);
        Memory::Pmm::free(q, 1);

        std::uint64_t r = Memory::Pmm::alloc(1);
        CHECK(r == p);
        Memory::Pmm::free(r, 1);
    }

    // A leak or a drift of one node per cycle wouldn't show up in a single
    // alloc/free pair, but it compounds: 256 cycles that keep landing on the
    // same page say the list is being put back exactly as it was found.
    TEST_CASE("repeated alloc/free cycles neither leak nor drift", "[pmm]") {
        std::uint64_t first = Memory::Pmm::alloc(1);
        REQUIRE(first != 0);
        Memory::Pmm::free(first, 1);

        bool stable = true;
        for (int i = 0; i < 256; ++i) {
            std::uint64_t pa = Memory::Pmm::alloc(1);
            if (pa != first) {
                stable = false;
            }
            Memory::Pmm::free(pa, 1);
        }
        CHECK(stable);

        // Multi-page cycles have to balance too -- the split path runs on
        // every one of these and pushes a remainder node back each time.
        std::uint64_t wide = Memory::Pmm::alloc(8);
        REQUIRE(wide != 0);
        Memory::Pmm::free(wide, 8);

        bool wide_stable = true;
        for (int i = 0; i < 64; ++i) {
            std::uint64_t pa = Memory::Pmm::alloc(8);
            if (pa != wide) {
                wide_stable = false;
            }
            Memory::Pmm::free(pa, 8);
        }
        CHECK(wide_stable);
    }

    // The freelist node lives inside the first page of the region it
    // describes, so alloc() hands back a page whose first bytes are a stale
    // node header. Scribbling on it is exactly what a caller does, and it
    // must not reach into the list the allocator is still using.
    TEST_CASE("writing over a fresh page's stale node header is harmless", "[pmm]") {
        std::uint64_t p = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);

        auto* words = page_at(p);
        for (int i = 0; i < 8; ++i) {
            words[i] = 0xDEADBEEFDEADBEEFull;   // covers next/num_pages/pa
        }

        // The allocator is unaffected: it still hands out usable pages.
        std::uint64_t q = Memory::Pmm::alloc(1);
        CHECK(q != 0);
        CHECK(q != p);

        Memory::Pmm::free(q, 1);
        Memory::Pmm::free(p, 1);
    }

    // A request smaller than the region splits it and pushes the tail back as
    // the new head, so the next single page comes from just past the run that
    // was carved off. Priming the head first makes the arithmetic exact.
    TEST_CASE("a split leaves the remainder at the head of the pool", "[pmm]") {
        std::uint64_t big = prime_head(8);
        REQUIRE(big != 0);                       // head: {big, 8}

        std::uint64_t p = Memory::Pmm::alloc(2); // takes big, pushes {big + 2 pages, 6}
        CHECK(p == big);

        std::uint64_t q = Memory::Pmm::alloc(1); // comes out of that remainder
        CHECK(q == p + 2 * PAGE);
        CHECK_FALSE(ranges_overlap(p, 2, q, 1));

        Memory::Pmm::free(q, 1);
        Memory::Pmm::free(p, 2);
    }

    TEST_CASE("an exact-fit request takes the whole region", "[pmm]") {
        std::uint64_t exact = prime_head(3);
        REQUIRE(exact != 0);                     // head: {exact, 3}

        // num_pages == count, so there is no remainder to push back and the
        // node behind it becomes the head.
        std::uint64_t p = Memory::Pmm::alloc(3);
        CHECK(p == exact);

        std::uint64_t next = Memory::Pmm::alloc(1);
        CHECK(next != 0);
        CHECK_FALSE(ranges_overlap(p, 3, next, 1));

        Memory::Pmm::free(next, 1);
        Memory::Pmm::free(p, 3);
    }

    // An impossible request has to come back empty rather than handing out a
    // short run, and it must leave the pool exactly as it found it.
    TEST_CASE("an oversized request fails without disturbing the pool", "[pmm]") {
        std::uint64_t before = prime_head(1);
        REQUIRE(before != 0);                    // head: {before, 1}

        const auto outstanding = Memory::Pmm::g_ref_alloc;

        CHECK(Memory::Pmm::alloc(1ull << 40) == 0);
        CHECK(Memory::Pmm::g_ref_alloc == outstanding);

        // The head is untouched, so the page that was there is still there.
        std::uint64_t p = Memory::Pmm::alloc(1);
        CHECK(p == before);
        Memory::Pmm::free(p, 1);
    }

    // -------------------------------------------------------------------
    // Misuse. g_ref_alloc is the number of allocations currently
    // outstanding: alloc() bumps it on success, free() drops it, and a free
    // that would take it below zero is a free with no matching alloc.
    //
    // Nothing in pmm.cpp touches g_ref_alloc yet, so the REQUIREs below are
    // load-bearing, not decoration: they abort each case before it performs
    // the bad free. An unmatched free() today writes a node header into a
    // region that is already on the list and links it to itself, which
    // strands the rest of RAM behind a self-referencing head -- these cases
    // must not run that for real until free() can turn it down.
    // -------------------------------------------------------------------

    TEST_CASE("alloc() and free() track the number of outstanding allocations", "[pmm]") {
        const auto base = Memory::Pmm::g_ref_alloc;

        std::uint64_t p = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        CHECK(Memory::Pmm::g_ref_alloc == base + 1);

        std::uint64_t q = Memory::Pmm::alloc(3);
        REQUIRE(q != 0);
        // One allocation is one count, whatever its size.
        CHECK(Memory::Pmm::g_ref_alloc == base + 2);

        Memory::Pmm::free(q, 3);
        CHECK(Memory::Pmm::g_ref_alloc == base + 1);

        Memory::Pmm::free(p, 1);
        CHECK(Memory::Pmm::g_ref_alloc == base);
    }

    TEST_CASE("free() with nothing outstanding is rejected", "[pmm]") {
        const auto base = Memory::Pmm::g_ref_alloc;

        std::uint64_t p = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        REQUIRE(Memory::Pmm::g_ref_alloc == base + 1);

        Memory::Pmm::free(p, 1);
        REQUIRE(Memory::Pmm::g_ref_alloc == base);

        // Second free of the same page: the count says there is nothing left
        // to give back, so this has to be turned down.
        Memory::Pmm::free(p, 1);
        CHECK(Memory::Pmm::g_ref_alloc == base);

        // And the page must appear in the pool once, not twice.
        std::uint64_t first  = Memory::Pmm::alloc(1);
        std::uint64_t second = Memory::Pmm::alloc(1);
        CHECK(first == p);
        CHECK(second != p);

        Memory::Pmm::free(second, 1);
        Memory::Pmm::free(first, 1);
    }

    TEST_CASE("free() of a null physical address is rejected", "[pmm]") {
        const auto base = Memory::Pmm::g_ref_alloc;

        std::uint64_t p = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        // Same guard as above, and this one is not theoretical: free(0, 1)
        // against the current code writes a node header at HHDM + 0, which
        // is not mapped, and the case dies in a #PF instead of failing.
        REQUIRE(Memory::Pmm::g_ref_alloc == base + 1);

        // 0 is the allocator's own out-of-memory sentinel, so a caller that
        // frees an alloc() result without checking it lands right here.
        Memory::Pmm::free(0, 1);
        CHECK(Memory::Pmm::g_ref_alloc == base + 1);   // p is still outstanding

        Memory::Pmm::free(p, 1);
        CHECK(Memory::Pmm::g_ref_alloc == base);

        // If the bogus free had been pushed, alloc() would hand back 0 and
        // every caller would read it as out of memory.
        std::uint64_t q = Memory::Pmm::alloc(1);
        CHECK(q != 0);
        Memory::Pmm::free(q, 1);
    }

    // count == 0 matches the head unconditionally (num_pages >= 0), unlinks
    // it, then pushes the identical region straight back -- so the caller
    // gets a page that is still on the freelist and will be handed out again.
    TEST_CASE("a zero-page request allocates nothing", "[pmm]") {
        std::uint64_t zero = Memory::Pmm::alloc(0);
        CHECK(zero == 0);

        std::uint64_t p = Memory::Pmm::alloc(1);
        REQUIRE(p != 0);
        CHECK(p != zero);                        // aliased if alloc(0) gave a page out

        Memory::Pmm::free(p, 1);
    }
}
