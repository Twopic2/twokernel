#ifndef __LIBC__STRING_HPP
#define __LIBC__STRING_HPP

#include <std/cstddef>

// Credit to mint's memory.hpp template repo
namespace LibC {
    extern "C" {
        void* memcpy(void* dest, void* src, std::size_t n);
        void* memset(void* dest, int c, std::size_t n);
        void* memmove(void* dest, void* src, std::size_t n);
        int memcmp(const void* s1, const void* s2, std::size_t n);
    }
}

#endif