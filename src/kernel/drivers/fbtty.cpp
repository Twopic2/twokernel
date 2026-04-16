#include <drivers/fbtty.hpp>

namespace Drivers {
    FbTty* g_tty = nullptr;

    FbTty::FbTty(struct limine_framebuffer* fb) : FrameBuffer(fb) {}

    void FbTty::write_terminal(const char* str, std::size_t len) {
        if (ft_ctx) {
            flanterm_write(ft_ctx, str, len);
        }
    }
}
