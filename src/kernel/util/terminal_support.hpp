#pragma once

#include <cstddef>

namespace Util::Terminal::Detail {
    inline constexpr char prompt[] = "TwoOs> ";
    void welcome();

    class Input {
        std::size_t length = 0;
        std::size_t limit;

    public:
        Input();
        void read_and_echo();
    };
}
