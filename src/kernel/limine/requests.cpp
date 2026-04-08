#include <limine/requests.hpp>
#include <std/cstdint>

#include <boot/boot.hpp>

namespace {
    __attribute__((used, section(".limine_requests")))
    volatile std::uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);
}

namespace Limine {
    __attribute__((used, section(".limine_requests")))
    volatile struct limine_framebuffer_request framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
        .revision = 0,
        .response = nullptr
    };
}

namespace {
    __attribute__((used, section(".limine_requests_start")))
    volatile std::uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

    __attribute__((used, section(".limine_requests_end")))
    volatile std::uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;
}

void Limine::base_revision_check(){
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        CxxRuntime::hcf();
    }
}