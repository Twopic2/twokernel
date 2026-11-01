#include "util/terminal.hpp"
#include "terminal_support.hpp"
#include "arch/x86-64/proc/scheduler.hpp"
#include "arch/x86-64/dev/keyboard.hpp"
#include "arch/x86-64/arch/event.hpp"
#include "drivers/fbtty.hpp"
#include "memory/heap.hpp"
#include "util/irq_gaurd.hpp"
#include "util/kernel_logger.hpp"

using namespace x86::Proc;

namespace Util::Terminal {
    void start() {
        IrqRAIGaurd irq {};
        auto* thread = new Thread::ThreadBlock {"terminal", &Scheduler::g_kernel_process, run, nullptr};
        Scheduler::g_kernel_process.add_thread(thread);
        Scheduler::g_scheduler.add_ready_threads(thread);
    }
}

namespace Util::Terminal::Detail {
    namespace {
        constexpr char keymap[] =
            "\0\0" "1234567890-=" "\0\0"
            "qwertyuiop[]" "\0\0"
            "asdfghjkl;'`" "\0\\"
            "zxcvbnm,./" "\0*" "\0 ";
        static_assert(sizeof(keymap) == 0x3A + 1);
    }

    void welcome() {
        klog("\x1b[2J\x1b[H\n\x1b[1;36m"
             "  ##### #   #  ###  #   # ##### ####  #   # ##### #\n"
             "    #   #   # #   # #  #  #     #   # ##  # #     #\n"
             "    #   # # # #   # ###   ####  ####  # # # ####  #\n"
             "    #   ## ## #   # #  #  #     #  #  #  ## #     #\n"
             "    #   #   #  ###  #   # ##### #   # #   # ##### #####\n"
             "\x1b[0m\n  welcome to twokernel\n\n%s", prompt);
    }

    Input::Input() {
        std::size_t columns = 0, rows = 0;
        flanterm_get_dimensions(Drivers::g_tty->ft_ctx, &columns, &rows);
        // Keep Backspace on the current row and outside the prompt.
        limit = columns > sizeof(prompt) ? columns - sizeof(prompt) : 0;
    }

    void Input::read_and_echo() {
        kstd::KeyboardBuffer& input = x86::Dev::Keyboard::device().data;
        Cpu::cli();
        while (input.is_empty()) {
            // The PIT can preempt this thread while it waits for keyboard input.
            asm volatile("sti; hlt; cli" ::: "memory");
        }
        const auto key = input.get_read();
        input.buf_read();
        Cpu::sti();

        const auto code = static_cast<Arch::ScanCode>(key);
        if (code == Arch::ScanCode::Enter || code == Arch::ScanCode::KeypadEnter) {
            klog("\n%s", prompt);
            length = 0;
        } else if (code == Arch::ScanCode::Backspace) {
            if (length != 0) {
                klog("\b \b");
                --length;
            }
        } else if (key < sizeof(keymap) - 1 && keymap[key] != '\0' && length < limit) {
            Drivers::g_tty->write_terminal(&keymap[key], 1);
            ++length;
        }
    }
}
