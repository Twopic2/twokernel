#pragma once

#include "util/kernel_logger.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace kstd {
    template< typename T, std::size_t N>
    struct RingBuffer {
        private:
            std::array<T, N> cap;
            std::size_t read {};
            std::size_t write {}; 
            std::size_t size {};

        public:
            void buf_read() {
                if (is_empty()) {
                    Util::klog_panic("buffer is empty can't read data");
                }
                
                read = (read + 1) % cap.size();  
                --size;
            } 

            void buf_write(T data) {
                if (is_full()) {
                    Util::klog_panic("buffer is full and can't add data");
                }

                cap[write] = data;
                write = (write + 1) % cap.size();
                size++;
            }

            T get_read() {
                return cap[read];
            }

            T get_write() {
                return cap[write];
            }
            
            bool is_full() const {
                return (write + 1) % cap.size() == read;
            }

            bool is_empty() const {
                return write == read;
            }

            std::size_t get_size() {
                return size;
            }
    };

    using KeyboardBuffer = RingBuffer<std::uint8_t, 255>;
}
