#include <util/kernel_logger.hpp>
#include <libc/string.hpp>
#include <drivers/fbtty.hpp>

namespace Util {
    char char_buff[4096];

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

                    if (!s) {
                        break;
                    }

                    auto len = LibC::strlen(s);
                    
                    for (int i = 0; i < len; i++) {
                        *str = *s;
                        ++s;
                        ++str;
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

    // Old version
    int printk(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        int len = vsnprintf(char_buff, fmt, args);

        va_end(args);

        return len;
    }

    void klog(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        int len = vsnprintf(char_buff, fmt, args);
        
        va_end(args);
        
        Drivers::g_tty->write_terminal(char_buff, len);
    }
}
