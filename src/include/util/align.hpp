#pragma once

#include <cstddef>
#include <cstdint>
namespace Util::Align {
    inline std::uint64_t align_up(std::uint64_t val, size_t page_size) {
        return (val + (page_size - 1)) & ~(page_size - 1);
    }

    inline uint64_t align_down(uint64_t val, size_t page_size) {
        return (val & ~(page_size - 1));
    }
}