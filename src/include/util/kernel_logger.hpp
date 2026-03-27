#pragma once

#include <libc/string.hpp>
#include <include/std/cstdarg>

namespace Util {
    extern char char_buff[4096];

    int printk(const char* fmt, ...);
    int vsnprintf(char *buf, const char *fmt, va_list args);
}
