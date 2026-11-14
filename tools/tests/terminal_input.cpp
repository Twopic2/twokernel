#include "drivers/fbtty.hpp"
#include "drivers/keyboard.hpp"
#include "drivers/ps2.hpp"
#include <cassert>
#include <cstdlib>
#include <string>

// A small display model with Flanterm's delayed wrap semantics.
struct flanterm_context {
    std::size_t x = 0, y = 0;
    char cells[25][80] {};
    std::string output;
};

namespace Util {
    void klog_panic(const char*) { std::abort(); }
}
namespace Drivers {
    FrameBuffer::FrameBuffer(limine_framebuffer* fb) : m_fb(fb) {}
}
extern "C" {
    void flanterm_write(flanterm_context* ctx, const char* bytes, std::size_t len) {
        assert(Util::locks_held != 0);
        ctx->output.append(bytes, len);
        for (std::size_t i = 0; i < len; ++i) {
            if (bytes[i] == '\r') ctx->x = 0;
            else if (bytes[i] == '\n') { if (ctx->y < 24) ++ctx->y; }
            else {
                if (ctx->x == 80) { ctx->x = 0; if (ctx->y < 24) ++ctx->y; }
                ctx->cells[ctx->y][ctx->x++] = bytes[i];
            }
        }
    }
    void flanterm_get_dimensions(flanterm_context*, std::size_t* columns, std::size_t* rows) {
        assert(Util::locks_held != 0);
        *columns = 80; *rows = 25;
    }
    void flanterm_get_cursor_pos(flanterm_context* ctx, std::size_t* x, std::size_t* y) {
        assert(Util::locks_held != 0);
        *x = ctx->x < 80 ? ctx->x : 79; *y = ctx->y;
    }
    void flanterm_set_cursor_pos(flanterm_context* ctx, std::size_t x, std::size_t y) {
        assert(Util::locks_held != 0);
        ctx->x = x; ctx->y = y;
    }
}

using Arch::ScanCode;
using Drivers::Keyboard::translate_key;

static std::string translated(Arch::KeyEvent event) {
    const auto sequence = translate_key(event);
    return {sequence.bytes, sequence.length};
}

static Arch::KeyEvent pop(Drivers::Ps2::Ps2Keyboard& keyboard) {
    assert(!keyboard.data.is_empty());
    const auto event = keyboard.data.get_read();
    keyboard.data.buf_read();
    return event;
}

static std::string read(Drivers::FbTty& tty, std::size_t limit = 2048) {
    char bytes[2048];
    const auto size = tty.read_input(bytes, limit);
    return {bytes, size};
}

static void send(Drivers::Ps2::Ps2Keyboard& keyboard, Drivers::FbTty& tty, std::uint8_t byte) {
    keyboard.on_recieve(byte);
    while (!keyboard.data.is_empty()) {
        const auto sequence = translate_key(pop(keyboard));
        tty.receive_input(sequence.bytes, sequence.length);
    }
}

static void decoder_and_modifiers() {
    Drivers::Ps2::Ps2Keyboard keyboard;
    keyboard.on_recieve(0x2A); // Shift down, A down, Shift up, B down before draining.
    keyboard.on_recieve(0x1E);
    keyboard.on_recieve(0xAA);
    keyboard.on_recieve(0x30);
    assert(translated(pop(keyboard)).empty());
    assert(translated(pop(keyboard)) == "A");
    assert(translated(pop(keyboard)) == "b");
    assert(keyboard.data.is_empty());

    keyboard.on_recieve(0x2A);
    keyboard.on_recieve(0x36);
    keyboard.on_recieve(0xAA); // Right Shift remains held.
    keyboard.on_recieve(0x02);
    pop(keyboard); pop(keyboard);
    assert(translated(pop(keyboard)) == "!");
    keyboard.on_recieve(0xB6);

    keyboard.on_recieve(0x3A); // Caps autorepeat must toggle only once.
    keyboard.on_recieve(0x3A);
    keyboard.on_recieve(0x1E);
    pop(keyboard); pop(keyboard);
    assert(translated(pop(keyboard)) == "A");
    keyboard.on_recieve(0xBA);
    keyboard.on_recieve(0x3A);
    keyboard.on_recieve(0x1E);
    pop(keyboard);
    assert(translated(pop(keyboard)) == "a");

    const std::uint8_t pause[] = {0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5};
    for (auto byte : pause) keyboard.on_recieve(byte);
    assert(keyboard.data.is_empty());
    assert(keyboard.modifiers == 0);
    const std::uint8_t print_screen[] = {0xE0, 0x2A, 0xE0, 0x37, 0xE0, 0xB7, 0xE0, 0xAA};
    for (auto byte : print_screen) keyboard.on_recieve(byte);
    assert(translated(pop(keyboard)).empty());
    assert(keyboard.modifiers == 0);

    keyboard.on_recieve(0xE0); keyboard.on_recieve(0x48);
    assert(translated(pop(keyboard)) == "\x1b[A");
    keyboard.on_recieve(0xE0); keyboard.on_recieve(0xC8);
    keyboard.on_recieve(0xFA);
    assert(keyboard.data.is_empty());
    assert(keyboard.signal == Drivers::Ps2::PS2Ack::Ack);

    // Saturating IRQ1 never panics or loses the modifier release state.
    for (unsigned i = 0; i < 1000; ++i) keyboard.on_recieve(0x1E);
    assert(keyboard.data.get_size() == 254);
    keyboard.on_recieve(0x2A); keyboard.on_recieve(0xAA);
    while (!keyboard.data.is_empty()) pop(keyboard);
    keyboard.on_recieve(0x30);
    assert(translated(pop(keyboard)) == "b");
}

static void layout_and_sequences() {
    assert(translated({ScanCode::A, Arch::CapsLock | Arch::LeftShift}) == "a");
    assert(translated({ScanCode::Num1, Arch::CapsLock}) == "1");
    assert(translated({ScanCode::Num1, Arch::LeftShift}) == "!");
    assert(translated({ScanCode::A, Arch::RightCtrl}) == std::string(1, '\x01'));
    assert(translated({ScanCode::Space, Arch::LeftCtrl}) == std::string(1, '\0'));
    assert(translated({ScanCode::X, Arch::LeftAlt}) == "\x1bx");
    assert(translated({ScanCode::Delete, 0}) == "\x1b[3~");
    assert(translated({ScanCode::Keypad8, 0}) == "\x1b[A");
    assert(translated({ScanCode::Keypad8, Arch::NumLock}) == "8");
    assert(translated({ScanCode::Keypad8, Arch::NumLock | Arch::RightShift}) == "\x1b[A");
    assert(translated({ScanCode::KeypadEnter, 0}) == "\n");
}

static void canonical_input() {
    Drivers::FbTty tty(nullptr);
    flanterm_context screen;
    tty.ft_ctx = &screen;
    tty.write_terminal("TwoOs> ", 7);
    Drivers::Ps2::Ps2Keyboard keyboard;
    send(keyboard, tty, 0x2A); send(keyboard, tty, 0x23); send(keyboard, tty, 0xAA);
    send(keyboard, tty, 0x17); // Hi
    assert(read(tty).empty());
    send(keyboard, tty, 0x1C);
    assert(read(tty, 1) == "H");
    assert(read(tty) == "i\n");
    assert(read(tty).empty());
    assert(screen.output == "TwoOs> Hi\r\n");

    tty.receive_input("\x7f", 1); // Cannot erase beyond the pending line.
    assert(screen.output == "TwoOs> Hi\r\n");
    tty.receive_input("hello worlx", 11);
    tty.receive_input("\x7f", 1);
    tty.receive_input("d\n", 2);
    assert(read(tty) == "hello world\n");
    tty.receive_input("one two\t", 8);
    tty.receive_input("\x17", 1);
    tty.receive_input("three\n", 6);
    assert(read(tty) == "one three\n");
    tty.receive_input("discard", 7);
    tty.receive_input("\x15", 1);
    tty.receive_input("ok\n", 3);
    assert(read(tty) == "ok\n");
    tty.receive_input("\x1b[A", 3); // No literal escape-sequence suffix in canonical input.
    tty.receive_input("\n", 1);
    assert(read(tty) == "\n");

    // Erase across a row boundary, then erase an expanded tab completely.
    screen.x = 79; screen.y = 10;
    tty.receive_input("x", 1);
    assert(screen.x == 0 && screen.y == 11);
    tty.receive_input("\x7f", 1);
    assert(screen.x == 79 && screen.y == 10 && screen.cells[10][79] == ' ');
    tty.receive_input("\t", 1);
    tty.receive_input("\x7f", 1);
    assert(screen.x == 79 && screen.y == 10);
    tty.receive_input("\n", 1);
    assert(read(tty) == "\n");
}

static void modes_and_capacity() {
    Drivers::FbTty tty(nullptr);
    assert(tty.set_input_mode(true, false));
    tty.receive_input("pending", 7);
    assert(read(tty).empty());
    assert(tty.set_input_mode(false, false));
    assert(read(tty) == "pending");
    tty.receive_input("\x1b[A", 3);
    const char nul = '\0';
    tty.receive_input(&nul, 1);
    assert(read(tty) == std::string("\x1b[A\0", 4));
    const std::string full(1023, 'a');
    tty.receive_input(full.data(), full.size());
    tty.receive_input("\x1b[A", 3); // Reject an entire sequence if it cannot fit.
    assert(read(tty) == full);

    assert(tty.set_input_mode(true, false));
    const std::string line(300, 'x');
    for (unsigned i = 0; i < 4; ++i) {
        tty.receive_input(line.data(), line.size());
        tty.receive_input("\n", 1);
    }
    tty.receive_input("retained\n", 9); // Full queue preserves the pending line.
    assert(!tty.set_input_mode(false, false));
    for (unsigned i = 0; i < 4; ++i) assert(read(tty) == std::string(255, 'x') + '\n');
    assert(read(tty).empty());
    tty.receive_input("\n", 1);
    assert(read(tty) == "retained\n");
}

int main() {
    decoder_and_modifiers();
    layout_and_sequences();
    canonical_input();
    modes_and_capacity();
    assert(Util::locks_held == 0);
}
