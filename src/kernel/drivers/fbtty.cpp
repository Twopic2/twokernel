#include <include/drivers/fbtty.hpp>

namespace Drivers {
    FbTty::FbTty(struct limine_framebuffer* fb) : FrameBuffer(fb) {}

    void FbTty::write_terminal(const char* str, std::size_t len) {
        if (ft_ctx) {
            flanterm_write(ft_ctx, str, len);
        }
    }

    void FbTty::putchar(char c) {
        write_terminal(&c, 1);
    }
}
