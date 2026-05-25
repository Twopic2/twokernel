#pragma once

/* 
Ansi Headers:
https://ss64.com/nt/syntax-ansi.html

website for color coding: 
https://ansi.tools/
*/

namespace Util::Ansi {
    // CSI = ESC '['
    inline constexpr const char* RESET         = "\x1b[0m";
    inline constexpr const char* BOLD          = "\x1b[1m";
    inline constexpr const char* DIM           = "\x1b[2m";
    inline constexpr const char* ITALIC        = "\x1b[3m";
    inline constexpr const char* UNDERLINE     = "\x1b[4m";
    inline constexpr const char* BLINK         = "\x1b[5m";
    inline constexpr const char* REVERSE       = "\x1b[7m";
    inline constexpr const char* HIDDEN        = "\x1b[8m";
    inline constexpr const char* STRIKETHROUGH = "\x1b[9m";

    // Per-attribute resets (leave other attributes intact)
    inline constexpr const char* RESET_BOLD_DIM       = "\x1b[22m";
    inline constexpr const char* RESET_ITALIC         = "\x1b[23m";
    inline constexpr const char* RESET_UNDERLINE      = "\x1b[24m";
    inline constexpr const char* RESET_BLINK          = "\x1b[25m";
    inline constexpr const char* RESET_REVERSE        = "\x1b[27m";
    inline constexpr const char* RESET_HIDDEN         = "\x1b[28m";
    inline constexpr const char* RESET_STRIKETHROUGH  = "\x1b[29m";

    // Foreground (normal)
    inline constexpr const char* BLACK   = "\x1b[30m";
    inline constexpr const char* RED     = "\x1b[31m";
    inline constexpr const char* GREEN   = "\x1b[32m";
    inline constexpr const char* YELLOW  = "\x1b[33m";
    inline constexpr const char* BLUE    = "\x1b[34m";
    inline constexpr const char* MAGENTA = "\x1b[35m";
    inline constexpr const char* CYAN    = "\x1b[36m";
    inline constexpr const char* WHITE   = "\x1b[37m";
    inline constexpr const char* DEFAULT = "\x1b[39m";

    // Foreground (bright)
    inline constexpr const char* BR_BLACK   = "\x1b[90m";
    inline constexpr const char* BR_RED     = "\x1b[91m";
    inline constexpr const char* BR_GREEN   = "\x1b[92m";
    inline constexpr const char* BR_YELLOW  = "\x1b[93m";
    inline constexpr const char* BR_BLUE    = "\x1b[94m";
    inline constexpr const char* BR_MAGENTA = "\x1b[95m";
    inline constexpr const char* BR_CYAN    = "\x1b[96m";
    inline constexpr const char* BR_WHITE   = "\x1b[97m";

    // Background (normal)
    inline constexpr const char* BG_BLACK   = "\x1b[40m";
    inline constexpr const char* BG_RED     = "\x1b[41m";
    inline constexpr const char* BG_GREEN   = "\x1b[42m";
    inline constexpr const char* BG_YELLOW  = "\x1b[43m";
    inline constexpr const char* BG_BLUE    = "\x1b[44m";
    inline constexpr const char* BG_MAGENTA = "\x1b[45m";
    inline constexpr const char* BG_CYAN    = "\x1b[46m";
    inline constexpr const char* BG_WHITE   = "\x1b[47m";
    inline constexpr const char* BG_DEFAULT = "\x1b[49m";

    // Background (bright)
    inline constexpr const char* BG_BR_BLACK   = "\x1b[100m";
    inline constexpr const char* BG_BR_RED     = "\x1b[101m";
    inline constexpr const char* BG_BR_GREEN   = "\x1b[102m";
    inline constexpr const char* BG_BR_YELLOW  = "\x1b[103m";
    inline constexpr const char* BG_BR_BLUE    = "\x1b[104m";
    inline constexpr const char* BG_BR_MAGENTA = "\x1b[105m";
    inline constexpr const char* BG_BR_CYAN    = "\x1b[106m";
    inline constexpr const char* BG_BR_WHITE   = "\x1b[107m";
}
