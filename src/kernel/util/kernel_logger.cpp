#include <include/util/kernel_logger.hpp>
#include <include/libc/string.hpp>

namespace Util {
    char char_buff[4096];

    int vsnprintf(char* buf, const char* fmt, va_list args) {
        char* str = buf;

        for (; *fmt; fmt++) {
            if (*fmt != '%') {
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

    int printk(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        int len = vsnprintf(char_buff, fmt, args);

        va_end(args);

        return len;
    }

}
