#include "memory/heap.hpp"
#include "limine/requests.hpp"
#include "memory/pmm.hpp"

[[nodiscard]] std::uint8_t* kmalloc(std::size_t kb) {
    auto pages = bytes_to_pages(kb + sizeof(KmallocHeader));
    auto pa = Memory::Pmm::alloc(pages);

    KmallocHeader* header = reinterpret_cast<KmallocHeader*>(pa + Limine::hhdm.response->offset);
    header->pages = pages;
    auto ptr = reinterpret_cast<std::uint8_t*>(header + 1);

    return ptr;
}

void kmalloc_free(std::uint8_t* k_ptr) {
    KmallocHeader* header = reinterpret_cast<KmallocHeader*>(k_ptr - 1);
    auto pa = reinterpret_cast<std::uint64_t>(k_ptr - Limine::get_hhdm());
    Memory::Pmm::free(pa, header->pages);
}

[[nodiscard]] void* operator new(std::size_t kb) {
    return kmalloc(kb);
}

void operator delete(void* ptr) noexcept {
    kmalloc_free(static_cast<std::uint8_t*>(ptr));
}

void operator delete(void* ptr, std::size_t) noexcept {
    ::operator delete(ptr);
}
