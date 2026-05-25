#include "libc/ctype.h"
#include <cstdint>
#include <string_view>
#include <util/kernel_logger.hpp>
#include <util/color.hpp>
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

    int atoi(std::string_view str) {
        int value {};
        std::size_t index = 0;

        while (LibC::isdigit(str[index])) {
            value = value * 10 + (str[index] - '0');
            index += 1;
        }

        return value;
    }

    static char* push_dec_u8(char* p, std::uint8_t v) {
        if (v >= 100) {
            *p++ = '0' + v / 100;
            *p++ = '0' + (v / 10) % 10;
            *p++ = '0' + v % 10;
        } else if (v >= 10) {
            *p++ = '0' + v / 10;
            *p++ = '0' + v % 10;
        } else {
            *p++ = '0' + v;
        }
        return p;
    }

    static char* push_fg_truecolor(char* p, std::uint32_t rgb) {
        auto ch = Color::rgb_to_value(rgb);
        *p++ = '\x1b'; *p++ = '['; *p++ = '3'; *p++ = '8';
        *p++ = ';';    *p++ = '2'; *p++ = ';';
        p = push_dec_u8(p, ch.r); *p++ = ';';
        p = push_dec_u8(p, ch.g); *p++ = ';';
        p = push_dec_u8(p, ch.b);
        *p++ = 'm';
        return p;
    }

    void klog_color(std::uint32_t rgb, const char* fmt, ...) {
        char buf[1024];
        char* p = buf;

        p = push_fg_truecolor(p, rgb);

        va_list args;
        va_start(args, fmt);
        p += vsnprintf(p, fmt, args);
        va_end(args);

        *p++ = '\x1b'; *p++ = '['; *p++ = '0'; *p++ = 'm';

        Drivers::g_tty->write_terminal(buf, p - buf);
    }
    
    void klog(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(buf, fmt, args);
        va_end(args);
        Drivers::g_tty->write_terminal(buf, len);
    }
}
