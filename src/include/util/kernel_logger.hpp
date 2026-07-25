#pragma once

#include "arch/x86-64/arch/arch_irq.hpp"
#include <cstdint>
#include <libc/string.hpp>
#include <libc/ctype.h>
#include <string_view>
#include <cstdarg>

namespace Util {
    int vsnprintf(char *buf, const char *fmt, va_list args);
    void klog_panic(const char* fmt);
    void klog(const char* fmt, ...);
    void klog_info(const char* fmt, ...);   
    void dump_registers(const char* label, const x86::ArchIrq::IrqFrame& frame);
}
