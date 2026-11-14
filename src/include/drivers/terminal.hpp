#pragma once

#include "arch/x86-64/arch/event.hpp"
#include "arch/x86-64/dev/keyboard.hpp"
#include "drivers/fbtty.hpp"
#include "drivers/keyboard.hpp"
#include "util/irq_gaurd.hpp"
#include "util/kernel_logger.hpp"

namespace Drivers::Terminal {
    // Queue the terminal once, after kernel setup and global construction.
    void start();
    [[noreturn]] void run(void*);

    namespace Detail {
        inline constexpr char prompt[] = "TwoOs> ";

        inline void welcome() {
            Util::klog("\x1b[2J\x1b[H\n\x1b[1;36m"
                       "  ##### #   #  ###  #   # ##### ####  #   # ##### #\n"
                       "    #   #   # #   # #  #  #     #   # ##  # #     #\n"
                       "    #   # # # #   # ###   ####  ####  # # # ####  #\n"
                       "    #   ## ## #   # #  #  #     #  #  #  ## #     #\n"
                       "    #   #   #  ###  #   # ##### #   # #   # ##### #####\n"
                       "\x1b[0m\n  welcome to twokernel\n\n%s", prompt);
        }

        inline Arch::KeyEvent wait_for_key() {
            Util::IrqRAIGaurd irq {};
            auto& input = x86::Dev::Keyboard::device().data;
            while (input.is_empty()) {
                // IRQ1 only queues events; rendering happens in this thread.
                asm volatile("sti; hlt; cli" ::: "memory");
            }
            const auto event = input.get_read();
            input.buf_read();
            return event;
        }

        inline void read_and_echo() {
            const auto sequence = Drivers::Keyboard::translate_key(wait_for_key());
            Drivers::g_tty->receive_input(sequence.bytes, sequence.length);

            // Until a shell takes over, consume complete lines and display the prompt.
            char line[256];
            const auto count = Drivers::g_tty->read_input(line, sizeof(line));
            if (count != 0 && line[count - 1] == '\n') {
                Util::klog("%s", prompt);
            }
        }
    }
}
