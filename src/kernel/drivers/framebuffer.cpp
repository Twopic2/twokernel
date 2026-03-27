#include <include/drivers/framebuffer.hpp>

namespace Drivers {
    FrameBuffer::FrameBuffer(struct limine_framebuffer* fb)
        : m_fb(fb) {
    }

    void FrameBuffer::fb_init() {
        ft_ctx = flanterm_fb_init(
            NULL, NULL,
            reinterpret_cast<std::uint32_t*>(m_fb->address),
            m_fb->width, m_fb->height, m_fb->pitch,
            m_fb->red_mask_size, m_fb->red_mask_shift,
            m_fb->green_mask_size, m_fb->green_mask_shift,
            m_fb->blue_mask_size, m_fb->blue_mask_shift,
            NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, 0, 0, 1,
            0, 0, 0, 0
        );
    }
}
