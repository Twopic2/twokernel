#include "arch/x86-64/arch/cpu.hpp"
#include "libc/ctype.h"
#include "util/ansi.hpp"
#include <cstdarg>
#include <cstdint>
#include <string_view>
#include <util/kernel_logger.hpp>
#include <libc/string.hpp>
#include <drivers/fbtty.hpp>

namespace Util {
    int vsnprintf(char* buf, const char* fmt, va_list args) {
        char* str = buf;

        for (; *fmt; fmt++) {
            if (*fmt != '%') {
                if (*fmt == '\n') {
                    *str++ = '\r';
                }
                *str++ = *fmt;
                continue;
            }

            fmt++;
            switch (*fmt) {
                case 's': {
                    auto s = va_arg(args, char *);
                    if (!s) break;
                    while (*s) {
                        *str++ = *s++;
                    }
                    break;
                }

                case 'd': {
                    int val = va_arg(args, int);
                    char tmp[20];
                    int i = 0;
                    if (val < 0) { *str++ = '-'; val = -val; }
                    if (val == 0) {
                        tmp[i++] = '0';
                    } else {
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                    }
                    while (i > 0) {
                        *str++ = tmp[--i];
                    }
                    break;
                }

                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    char tmp[20];
                    int i = 0;
                    if (val == 0) {
                        tmp[i++] = '0';
                    } else {
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                    }
                    while (i > 0) {
                        *str++ = tmp[--i];
                    }
                    break;
                }

                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char tmp[16];
                    int i = 0;
                    if (val == 0) {
                        tmp[i++] = '0';
                    } else {
                        while (val > 0) {
                            int nibble = val & 0xF;
                            tmp[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
                            val >>= 4;
                        }
                    }
                    while (i > 0) {
                        *str++ = tmp[--i];
                    }
                    break;
                }

                case 'l': {
                    if (fmt[1] == 'l') {
                        fmt += 2;
                        if (*fmt == 'x') {
                            unsigned long long val = va_arg(args, unsigned long long);
                            char tmp[16];
                            int i = 0;
                            if (val == 0) {
                                tmp[i++] = '0';
                            } else {
                                while (val > 0) {
                                    int nibble = val & 0xF;
                                    tmp[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
                                    val >>= 4;
                                }
                            }
                            while (i > 0) *str++ = tmp[--i];
                        } else if (*fmt == 'u') {
                            unsigned long long val = va_arg(args, unsigned long long);
                            char tmp[20];
                            int i = 0;
                            if (val == 0) {
                                tmp[i++] = '0';
                            } else {
                                while (val > 0) {
                                    tmp[i++] = '0' + (val % 10);
                                    val /= 10;
                                }
                            }
                            while (i > 0) *str++ = tmp[--i];
                        }
                    }
                    break;
                }

                case '%':
                    *str++ = '%';
                    break;
                default:
                    *str++ = '%';
                    
                    if (*fmt) {
                        *str++ = *fmt;
                    } else {
                        --fmt;
                    }

                    break;
            }
        }

        *str = '\0';
        return static_cast<int>(str - buf);
    }

    void klog(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buf, fmt, args);
        va_end(args);
        Drivers::g_tty->write_terminal(buf, len);
    }

    void klog_panic(const char *fmt) {
        klog("%s%s[kernel]: PANIC%s\n", BOLD, RED, RESET);
        klog("%s", fmt);
        
        Cpu::cli();

        while (true) {
            asm volatile("hlt");
        }
    }
}
