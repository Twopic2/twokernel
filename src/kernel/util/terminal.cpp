#include "util/terminal.hpp"
#include "terminal_support.hpp"

namespace Util::Terminal {
    void run(void*) {
        Detail::Input input;
        Detail::welcome();
        for (;;) input.read_and_echo();
    }
}
