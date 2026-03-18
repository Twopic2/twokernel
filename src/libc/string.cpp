#include <libc/string.hpp>

#include <std/cstdint>

namespace LibC {
    extern "C" {
        void* memcpy(void* dest, void* src, std::size_t n) {
            std::uint8_t* p_dest = reinterpret_cast<std::uint8_t* >(dest);
            const std::uint8_t* p_src = reinterpret_cast<std::uint8_t* >(src); 

            for (std::size_t i = 0; i < n; i++) {
                p_dest[i] = p_src[i]; 
            }

            return dest;
        }

        void* memset(void* s, int c, std::size_t n) {
            std::uint8_t *p = reinterpret_cast<std::uint8_t *>(s);

            for (std::size_t i = 0; i < n; i++) {
                p[i] = static_cast<uint8_t>(c);
            }

            return s;
        }
    }
}