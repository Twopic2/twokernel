#include <drivers/fbtty.hpp>

namespace Drivers {
    namespace {
        struct TtyGuard {
            Util::IrqSpinLock& lock;
            explicit TtyGuard(Util::IrqSpinLock& value) : lock(value) { lock.acquire(); }
            ~TtyGuard() { lock.release(); }
        };
    }

    FbTty* g_tty = nullptr;

    FbTty::FbTty(struct limine_framebuffer* fb) : FrameBuffer(fb) {}

    void FbTty::write_terminal(const char* str, std::size_t len) {
        TtyGuard guard(lock_);
        write_locked(str, len);
    }

    void FbTty::write_locked(const char* str, std::size_t len) {
        if (ft_ctx) {
            flanterm_write(ft_ctx, str, len);
        }
    }

    void FbTty::dimensions(std::size_t& columns, std::size_t& rows) {
        TtyGuard guard(lock_);
        columns = rows = 0;
        if (ft_ctx) flanterm_get_dimensions(ft_ctx, &columns, &rows);
    }

    void FbTty::echo_character(char ch) {
        if (!echo_ || !ft_ctx) return;
        std::size_t x, y, columns, rows;
        flanterm_get_cursor_pos(ft_ctx, &x, &y);
        flanterm_get_dimensions(ft_ctx, &columns, &rows);
        write_locked(&ch, 1);
        // Resolve delayed wrapping now so erasing at a row boundary is unambiguous.
        if (columns != 0 && x + 1 == columns) write_locked("\r\n", 2);
    }

    void FbTty::erase_character() {
        if (line_length_ == 0) return;
        --line_length_;
        if (!echo_ || !ft_ctx) return;
        for (unsigned i = 0; i < widths_[line_length_]; ++i) {
            std::size_t x, y, columns, rows;
            flanterm_get_cursor_pos(ft_ctx, &x, &y);
            flanterm_get_dimensions(ft_ctx, &columns, &rows);
            if (x > 0) --x;
            else if (y > 0 && columns > 0) { --y; x = columns - 1; }
            else break;
            flanterm_set_cursor_pos(ft_ctx, x, y);
            write_locked(" ", 1);
            flanterm_set_cursor_pos(ft_ctx, x, y);
        }
        write_locked("", 0);
    }

    bool FbTty::commit_line(bool newline) {
        if (1024 - input_.get_size() < line_length_ + (newline ? 1 : 0)) return false;
        for (std::size_t i = 0; i < line_length_; ++i) input_.buf_write(line_[i]);
        if (newline) input_.buf_write('\n');
        line_length_ = 0;
        return true;
    }

    bool FbTty::set_input_mode(bool canonical, bool echo) {
        TtyGuard guard(lock_);
        // Preserve pending canonical text when a reader switches to raw input.
        if (!canonical && !commit_line(false)) return false;
        canonical_ = canonical;
        echo_ = echo;
        return true;
    }

    void FbTty::receive_input(const char* bytes, std::size_t len) {
        TtyGuard guard(lock_);
        if (!canonical_) {
            if (len > 1024 - input_.get_size()) return;
            for (std::size_t i = 0; i < len; ++i) input_.buf_write(bytes[i]);
            if (echo_) write_locked(bytes, len);
            return;
        }
        // Navigation and Alt sequences belong to raw-mode applications.
        if (len > 1 && bytes[0] == '\x1b') return;
        for (std::size_t i = 0; i < len; ++i) {
            const char ch = bytes[i];
            if (ch == '\n' || ch == '\r') {
                if (commit_line(true) && echo_) write_locked("\r\n", 2);
            } else if (ch == '\b' || ch == '\x7f') {
                erase_character();
            } else if (ch == '\x15') { // Ctrl+U: erase the pending line.
                while (line_length_ != 0) erase_character();
            } else if (ch == '\x17') { // Ctrl+W: erase the previous word.
                while (line_length_ && (line_[line_length_ - 1] == ' ' || line_[line_length_ - 1] == '\t')) erase_character();
                while (line_length_ && line_[line_length_ - 1] != ' ' && line_[line_length_ - 1] != '\t') erase_character();
            } else if ((ch >= ' ' && ch <= '~') || ch == '\t') {
                if (line_length_ == sizeof(line_) - 1) continue;
                unsigned width = 1;
                if (ch == '\t') {
                    std::size_t x = 0, y = 0;
                    if (ft_ctx) flanterm_get_cursor_pos(ft_ctx, &x, &y);
                    width = 8 - x % 8;
                }
                line_[line_length_] = ch;
                widths_[line_length_++] = width;
                for (unsigned j = 0; j < width; ++j) echo_character(ch == '\t' ? ' ' : ch);
            }
        }
    }

    std::size_t FbTty::read_input(char* bytes, std::size_t len) {
        TtyGuard guard(lock_);
        std::size_t count = 0;
        while (count < len && !input_.is_empty()) {
            const char ch = input_.get_read();
            input_.buf_read();
            bytes[count++] = ch;
            if (canonical_ && ch == '\n') break;
        }
        return count;
    }
}
