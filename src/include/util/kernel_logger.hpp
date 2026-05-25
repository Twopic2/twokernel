#pragma once

#include <cstdint>
#include <libc/string.hpp>
#include <libc/ctype.h>
#include <std/string_view>
#include <cstdarg>
#include <string_view>

namespace Util {
    int atoi(std::string_view str);

    int printk(const char* fmt, ...);
    int vsnprintf(char *buf, const char *fmt, va_list args);
    
    void klog(const char* fmt, ...);
    void klog_color(std::uint32_t color, const char* fmt, ...);
    void klog_info(const char* fmt, ...);
}
