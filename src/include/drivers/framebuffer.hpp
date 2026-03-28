#pragma once

#include <limine.h>
#include <cstddef>
#include <cstdint>

extern "C" {
    #include <flanterm.h>
    #include <flanterm_backends/fb.h>
}

namespace Drivers {
    class FrameBuffer {
    protected:
        struct limine_framebuffer* m_fb {};
        struct flanterm_context* ft_ctx {};

    public:
        FrameBuffer(struct limine_framebuffer* fb);
        ~FrameBuffer() = default;

        void fb_init();
    };
}
