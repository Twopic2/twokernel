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

        void* memove(void* dest, const void* src, std::size_t n) {
            std::uint8_t* pdest = static_cast<std::uint8_t *>(dest);
            const std::uint8_t* psrc = static_cast<const std::uint8_t *>(src);
            
            if (reinterpret_cast<std::uintptr_t>(src) > reinterpret_cast<std::uintptr_t>(dest)) {
                for (std::size_t i = 0; i < n; i++) {
                    pdest[i] = psrc[i];
                }
            } else if (reinterpret_cast<std::uintptr_t>(src) < reinterpret_cast<std::uintptr_t>(dest)) {
                for (std::size_t i = n; i > 0; i--) {
                    pdest[i-1] = psrc[i-1];
                }
            }
            return pdest;
        }

        int memcmp(const void* s1, const void* s2, std::size_t n) {
            const std::uint8_t *p1 = static_cast<const std::uint8_t *>(s1);
            const std::uint8_t *p2 = static_cast<const std::uint8_t *>(s2);

            for (std::size_t i = 0; i < n; i++) {
                if (p1[i] != p2[i]) {
                    return p1[i] < p2[i] ? -1 : 1;
                }
            }

            return 0;
        }
    }
}
