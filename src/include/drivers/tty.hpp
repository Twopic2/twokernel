#pragma once

#include <limine.h>
#include <drivers/font_8x16.hpp>
#include <drivers/framebuffer.hpp>
#include <std/string_view>

// Ansi color codes
// https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124

namespace Drivers {
    /// @brief Credit given to RaidTheWeb
    struct cc {
        uint8_t vintr; // CTRL+C.
        uint8_t vquit; /* CTRL+\. */
        uint8_t verase; // Backspace.
        uint8_t vkill; // Ctrl+U.
        uint8_t veof; // Ctrl+D.
        uint8_t vtime; // Non-canon read timeout.
        uint8_t vmin; // Minimum for non-canon read.
        uint8_t vswtch; // Switch character.
        uint8_t vstart; // CTRL+Q.
        uint8_t vstop; // CTRL+S.
        uint8_t vsusp; // CTRL+Z.
        uint8_t veol; // EOL alt.
        uint8_t vreprint; // CTRL+R.
        uint8_t vdiscard; // CTRL+O.
        uint8_t vwerase; // CTRL+W.
        uint8_t vlnext; // CTRL+V.
        uint8_t veol2;
        uint8_t padding[17];
    };

    class Tty : FrameBuffer {
        private:
            struct FontPixelSize {
                uint8_t col;
                uint8_t row;
            };

            static constexpr FontPixelSize default_size { 8, 16 };

            ColorRGB m_fg { 0xAA, 0xAA, 0xAA }; // light gray
            ColorRGB m_bg { 0x00, 0x00, 0x00 }; // black

            std::uint64_t m_cursor_x = 0;
            std::uint64_t m_cursor_y = 0;
            std::uint64_t m_cols;
            std::uint64_t m_rows;

            void draw_char(char c, ColorRGB& fg, ColorRGB& bg);
            /// TODO: make sure to add a circular buffer(deque) for keyword presses and line lookup
            // 4096 for both read/write
            
        public:
            Tty(struct limine_framebuffer* fb);
   
            void write_terminal(std::string_view str);

            void putchar(char c);
            void newline();
    };
}
