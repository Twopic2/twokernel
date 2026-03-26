#include <include/drivers/tty.hpp>

namespace Drivers {
    Tty::Tty(struct limine_framebuffer* fb) : FrameBuffer(fb) {
        m_cols = fb->width / default_size.col;
        m_rows = fb->height / default_size.row;
    }

    void Tty::write_terminal(std::string_view str) {
        for (char c : str) {
            putchar(c);
        }
    }

    void Tty::putchar(char c) {
        switch (c) {
        case '\n':
            newline();
            break;
        case '\r':
            m_cursor_x = 0;
            break;
        case '\t':
            m_cursor_x = (m_cursor_x + 4) & ~3u;
            if (m_cursor_x >= m_cols) newline();
            break;
        default:
            draw_char(c, m_fg, m_bg);
            m_cursor_x++;
            if (m_cursor_x >= m_cols) newline();
            break;
        }
    }

    void Tty::draw_char(char c, ColorRGB& fg, ColorRGB& bg) {
        std::uint64_t px = m_cursor_x * default_size.col;
        std::uint64_t py = m_cursor_y * default_size.row;
        const std::uint8_t* glyph = font_8x16[static_cast<std::uint8_t>(c)];

        auto fg_color = static_cast<std::uint32_t>(get_addr(fg));
        auto bg_color = static_cast<std::uint32_t>(get_addr(bg));

        for (std::uint8_t row = 0; row < default_size.row; row++) {
            for (std::uint8_t col = 0; col < default_size.col; col++) {
                bool lit = glyph[row] & (1 << (7 - col));
                //  0 encodes background, 1 encodes foreground color
                put_pixel(px + col, py + row, lit ? fg_color : bg_color);
            }
        }
    }

    void Tty::newline() {
        m_cursor_x = 0;
        m_cursor_y++;
        if (m_cursor_y >= m_rows) {
            m_cursor_y = m_rows - 1;
        }
    }

}
