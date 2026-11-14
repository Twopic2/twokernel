#pragma once

#include <limine.h>
#include <drivers/framebuffer.hpp>
#include <cstddef>
#include "std/ring_buffer.hpp"
#include "util/spinlocks.hpp"

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

    class FbTty : public FrameBuffer {
        public:
            FbTty(struct limine_framebuffer* fb);

            void write_terminal(const char* str, std::size_t len);
            void dimensions(std::size_t& columns, std::size_t& rows);

            // Input is nonblocking. Canonical reads expose only completed lines.
            void receive_input(const char* bytes, std::size_t len);
            std::size_t read_input(char* bytes, std::size_t len);
            bool set_input_mode(bool canonical, bool echo);

        private:
            Util::IrqSpinLock lock_;
            kstd::RingBuffer<char, 1025> input_;
            char line_[256] {};
            std::uint8_t widths_[256] {};
            std::size_t line_length_ = 0;
            bool canonical_ = true;
            bool echo_ = true;

            void write_locked(const char* bytes, std::size_t len);
            void echo_character(char ch);
            void erase_character();
            bool commit_line(bool newline);
    };

    extern FbTty* g_tty;
}
