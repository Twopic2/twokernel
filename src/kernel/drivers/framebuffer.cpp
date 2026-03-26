#include <include/drivers/framebuffer.hpp>

namespace Drivers {
    FrameBuffer::FrameBuffer(struct limine_framebuffer* fb_limine)
        : m_fb(fb_limine) {
    }  

    void FrameBuffer::put_pixel(std::uint64_t x, std::uint64_t y, const ColorRGB& color) {
        put_pixel(x, y, static_cast<std::uint32_t>(get_addr(color)));
    }

    void FrameBuffer::put_pixel(std::uint64_t x, std::uint64_t y, std::uint32_t packed_color) {
        int pixel_width = m_fb->bpp / 8;
        std::uint64_t where = y * m_fb->pitch + x * pixel_width;

        auto* screen = static_cast<std::uint8_t*>(m_fb->address);
        *reinterpret_cast<std::uint32_t*>(screen + where) = packed_color;
    }

    void FrameBuffer::fill_rect(std::uint64_t width, std::uint64_t height, const ColorRGB& color) {
        std::uint32_t pixel = get_addr(color);
        auto* where = reinterpret_cast<std::uint8_t*>(m_fb->address);
        int pixel_width = m_fb->bpp / 8;

        for (std::uint64_t i = 0; i < height; i++) {
            for (std::uint64_t j = 0; j < width; j++) {
                *reinterpret_cast<std::uint32_t*>(where + j * pixel_width) = pixel;
            }
            where += m_fb->pitch;
        }
    }
}