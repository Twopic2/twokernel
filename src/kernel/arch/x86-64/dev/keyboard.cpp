#include "arch/x86-64/dev/keyboard.hpp"
#include "arch/x86-64/system/irq.hpp"
#include "drivers/ps2.hpp"
#include "util/kernel_logger.hpp"
#include "util/ansi.hpp"

namespace x86::Dev::Keyboard {
    namespace {
        Drivers::Ps2::Ps2Keyboard g_keyboard;
    }
    void isr_keyboard(ArchIrq::IrqFrame*) {
        g_keyboard.on_recieve(g_keyboard.read_data());

        System::Irq::eoi(1);
    }

    void keyboard_init() {
        Util::klog("%s%s[init] Keyboard %s\n", BOLD, RED, RESET);
        g_keyboard.ps2_keyboard_init();
        System::Irq::irq_unmask(1);
        Util::klog("keyboard: IRQ1 unmasked\n");
    }

    Drivers::Ps2::Ps2Keyboard& device() {
        return g_keyboard;
    }

    void reset() {
        g_keyboard.write_cmd(Drivers::Ps2::CPU_RESET);
    }
}
