#pragma once

#include <cstdint>

/**
    @note: Whenever a key is being pressed/released keyboard sends some data via the databus
*/

namespace Arch {
    enum class ScanCode : std::uint8_t {
        Escape       = 0x01,
        Num1         = 0x02,
        Num2         = 0x03,
        Num3         = 0x04,
        Num4         = 0x05,
        Num5         = 0x06,
        Num6         = 0x07,
        Num7         = 0x08,
        Num8         = 0x09,
        Num9         = 0x0A,
        Num0         = 0x0B,
        Minus        = 0x0C,
        Equals       = 0x0D,
        Backspace    = 0x0E,
        Tab          = 0x0F,
        Q            = 0x10,
        W            = 0x11,
        E            = 0x12,
        R            = 0x13,
        T            = 0x14,
        Y            = 0x15,
        U            = 0x16,
        I            = 0x17,
        O            = 0x18,
        P            = 0x19,
        LeftBracket  = 0x1A,
        RightBracket = 0x1B,
        Enter        = 0x1C,
        LeftCtrl     = 0x1D,
        A            = 0x1E,
        S            = 0x1F,
        D            = 0x20,
        F            = 0x21,
        G            = 0x22,
        H            = 0x23,
        J            = 0x24,
        K            = 0x25,
        L            = 0x26,
        Semicolon    = 0x27,
        Apostrophe   = 0x28,
        Backtick     = 0x29,
        LeftShift    = 0x2A,
        Backslash    = 0x2B,
        Z            = 0x2C,
        X            = 0x2D,
        C            = 0x2E,
        V            = 0x2F,
        B            = 0x30,
        N            = 0x31,
        M            = 0x32,
        Comma        = 0x33,
        Period       = 0x34,
        Slash        = 0x35,
        RightShift   = 0x36,
        KeypadStar   = 0x37,
        LeftAlt      = 0x38,
        Space        = 0x39,
        CapsLock     = 0x3A,
        F1           = 0x3B,
        F2           = 0x3C,
        F3           = 0x3D,
        F4           = 0x3E,
        F5           = 0x3F,
        F6           = 0x40,
        F7           = 0x41,
        F8           = 0x42,
        F9           = 0x43,
        F10          = 0x44,
        NumLock      = 0x45,
        ScrollLock   = 0x46,
        Keypad7      = 0x47,
        Keypad8      = 0x48,
        Keypad9      = 0x49,
        KeypadMinus  = 0x4A,
        Keypad4      = 0x4B,
        Keypad5      = 0x4C,
        Keypad6      = 0x4D,
        KeypadPlus   = 0x4E,
        Keypad1      = 0x4F,
        Keypad2      = 0x50,
        Keypad3      = 0x51,
        Keypad0      = 0x52,
        KeypadPeriod = 0x53,
        F11          = 0x57,
        F12          = 0x58,

        KeypadEnter  = 0x9C,
        RightCtrl    = 0x9D,
        KeypadSlash  = 0xB5,
        RightAlt     = 0xB8,
        Home         = 0xC7,
        Up           = 0xC8,
        PageUp       = 0xC9,
        Left         = 0xCB,
        Right        = 0xCD,
        End          = 0xCF,
        Down         = 0xD0,
        PageDown     = 0xD1,
        Insert       = 0xD2,
        Delete       = 0xD3,
    };

    inline constexpr bool ps2_is_release(std::uint8_t byte) {
        return byte & 0x80;
    }

    inline constexpr ScanCode ps2_single_byte(std::uint8_t byte) {
        return static_cast<ScanCode>(byte & 0x7F);
    }

    inline constexpr ScanCode ps2_extend_codes(std::uint8_t byte) {
        return static_cast<ScanCode>(0x80 | (byte & 0x7F));
    }
}
