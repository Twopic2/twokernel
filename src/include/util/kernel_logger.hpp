#pragma once

#include <libc/string.hpp>
#include <include/std/cstdarg>
#include <include/std/array>

namespace Util {
    inline std::array<char, 4096> char_buff;

    int printk(const char* fmt, ...);
    int vsnprintf(char *buf, const char *fmt, va_list args);

}