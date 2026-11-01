#include <memory/heap.hpp>
#include <memory/pmm.hpp>
#include <memory/vmm.hpp>
#include <limine/requests.hpp>
#include <tests/ktest.hpp>
#include <new>

namespace Tests {
    namespace {
        // Release allocations even when REQUIRE returns early.
        struct HeapBuffer {
            std::uint8_t* ptr;

            explicit HeapBuffer(std::size_t bytes) : ptr(kmalloc(bytes)) {}
            ~HeapBuffer() { kmalloc_free(ptr); }

            HeapBuffer(const HeapBuffer&) = delete;
            HeapBuffer& operator=(const HeapBuffer&) = delete;
        };
    }

    TEST_CASE("heap returns aligned writable memory", "[heap]") {
        const auto before = Memory::Pmm::g_ref_alloc;
        {
            HeapBuffer buffer(64);
            REQUIRE(buffer.ptr != nullptr);
            CHECK(reinterpret_cast<std::uintptr_t>(buffer.ptr) % alignof(std::max_align_t) == 0);

            for (std::size_t i = 0; i < 64; ++i) {
                buffer.ptr[i] = static_cast<std::uint8_t>(i);
            }
            bool intact = true;
            for (std::size_t i = 0; i < 64; ++i) {
                intact &= buffer.ptr[i] == static_cast<std::uint8_t>(i);
            }
            CHECK(intact);
        }
        CHECK(Memory::Pmm::g_ref_alloc == before);
    }

    TEST_CASE("heap reserves space for the header at page boundaries", "[heap]") {
        constexpr auto page = Memory::Vmm::PAGE_SIZE;
        struct Boundary {
            std::size_t bytes;
            std::uint64_t pages;
        };
        const Boundary boundaries[] = {
            {1, 1},
            {page - sizeof(KmallocHeader), 1},
            {page - sizeof(KmallocHeader) + 1, 2},
            {page, 2},
            {page + 1, 2},
            {10 * 1024, 3},
        };

        const auto before = Memory::Pmm::g_ref_alloc;
        for (const auto& boundary : boundaries) {
            HeapBuffer buffer(boundary.bytes);
            REQUIRE(buffer.ptr != nullptr);
            const auto* header = reinterpret_cast<const KmallocHeader*>(buffer.ptr) - 1;
            CHECK(header->pages == boundary.pages);
            buffer.ptr[0] = 0x12;
            buffer.ptr[boundary.bytes - 1] = 0x34;
            CHECK(buffer.ptr[boundary.bytes - 1] == 0x34);
        }
        CHECK(Memory::Pmm::g_ref_alloc == before);
    }

    TEST_CASE("heap multi-page buffers do not overlap or overwrite each other", "[heap]") {
        constexpr std::size_t bytes = 10 * 1024;
        const auto before = Memory::Pmm::g_ref_alloc;
        {
            HeapBuffer first(bytes);
            HeapBuffer second(bytes);
            REQUIRE(first.ptr != nullptr);
            REQUIRE(second.ptr != nullptr);

            const auto a = reinterpret_cast<std::uintptr_t>(first.ptr);
            const auto b = reinterpret_cast<std::uintptr_t>(second.ptr);
            CHECK(a + bytes <= b || b + bytes <= a);

            for (std::size_t i = 0; i < bytes; ++i) {
                first.ptr[i] = static_cast<std::uint8_t>(i);
                second.ptr[i] = static_cast<std::uint8_t>(~i);
            }
            bool intact = true;
            for (std::size_t i = 0; i < bytes; ++i) {
                intact &= first.ptr[i] == static_cast<std::uint8_t>(i);
                intact &= second.ptr[i] == static_cast<std::uint8_t>(~i);
            }
            CHECK(intact);
        }
        CHECK(Memory::Pmm::g_ref_alloc == before);
    }

    TEST_CASE("heap free returns the original physical run to the PMM", "[heap]") {
        const auto before = Memory::Pmm::g_ref_alloc;
        std::uintptr_t base;
        {
            HeapBuffer buffer(10 * 1024);
            REQUIRE(buffer.ptr != nullptr);
            base = reinterpret_cast<std::uintptr_t>(buffer.ptr - sizeof(KmallocHeader))
                 - Limine::get_hhdm();
        }
        CHECK(Memory::Pmm::g_ref_alloc == before);

        // The current PMM returns its most recently freed suitable run first.
        const auto pa = Memory::Pmm::alloc(3);
        REQUIRE(pa != 0);
        CHECK(pa == base);
        CHECK(pa % Memory::Vmm::PAGE_SIZE == 0);
        Memory::Pmm::free(pa, 3);
        CHECK(Memory::Pmm::g_ref_alloc == before);
    }

    TEST_CASE("heap rejects invalid sizes and out of memory without allocating", "[heap]") {
        const auto before = Memory::Pmm::g_ref_alloc;
        HeapBuffer zero(0);
        HeapBuffer overflow(static_cast<std::size_t>(-1));
        HeapBuffer unavailable(std::size_t{1} << 48);
        CHECK(zero.ptr == nullptr);
        CHECK(overflow.ptr == nullptr);
        CHECK(unavailable.ptr == nullptr);
        kmalloc_free(nullptr);
        CHECK(Memory::Pmm::g_ref_alloc == before);
    }

    TEST_CASE("heap new and delete construct and destroy a multi-page object", "[heap]") {
        struct alignas(16) Object {
            bool* destroyed;
            std::uint8_t payload[10 * 1024];

            explicit Object(bool* flag) : destroyed(flag), payload{} {
                payload[0] = 0x56;
                payload[sizeof(payload) - 1] = 0x78;
            }
            ~Object() { *destroyed = true; }
        };

        const auto before = Memory::Pmm::g_ref_alloc;
        bool destroyed = false;
        auto* object = new Object(&destroyed);
        REQUIRE(object != nullptr);
        CHECK(reinterpret_cast<std::uintptr_t>(object) % alignof(Object) == 0);
        CHECK(object->payload[0] == 0x56);
        CHECK(object->payload[sizeof(object->payload) - 1] == 0x78);
        CHECK_FALSE(destroyed);
        delete object;
        CHECK(destroyed);
        CHECK(Memory::Pmm::g_ref_alloc == before);
    }
}
