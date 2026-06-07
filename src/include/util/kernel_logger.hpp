#pragma once

#include <cstdint>
#include <libc/string.hpp>
#include <libc/ctype.h>
#include <std/string_view>
#include <cstdarg>

namespace Util {
    int vsnprintf(char *buf, const char *fmt, va_list args);
    
    void klog(const char* fmt, ...);
    void klog_info(const char* fmt, ...);
}
