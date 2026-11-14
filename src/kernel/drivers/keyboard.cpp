#include "drivers/keyboard.hpp"

namespace Drivers::Keyboard {
    namespace {
        char translate_character(Arch::KeyEvent event) {
            constexpr char plain[] =
                "\0\x1b" "1234567890-=\x7f\t"
                "qwertyuiop[]\n\0"
                "asdfghjkl;'`\0\\"
                "zxcvbnm,./\0*\0 ";
            constexpr char shifted[] =
                "\0\x1b" "!@#$%^&*()_+\x7f\t"
                "QWERTYUIOP{}\n\0"
                "ASDFGHJKL:\"~\0|"
                "ZXCVBNM<>?\0*\0 ";
            static_assert(sizeof(plain) == sizeof(shifted));
            static_assert(sizeof(plain) == 0x3A + 1);

            const auto key = static_cast<unsigned>(event.code);
            if (key >= sizeof(plain) - 1) {
                switch (event.code) {
                    case Arch::ScanCode::KeypadEnter: return '\n';
                    case Arch::ScanCode::KeypadSlash: return '/';
                    default: return 0;
                }
            }

            char ch = event.shift() ? shifted[key] : plain[key];
            if ((event.modifiers & Arch::CapsLock) && plain[key] >= 'a' && plain[key] <= 'z') {
                ch = event.shift() ? plain[key] : shifted[key];
            }
            return ch;
        }

        char control_character(char ch) {
            if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
            if (ch >= '@' && ch <= '_') return ch & 0x1F;
            if (ch == ' ' || ch == '2') return '\0';
            if (ch == '?') return '\x7f';
            return ch;
        }

        KeySequence character_sequence(char ch, Arch::KeyEvent event) {
            KeySequence result;
            if (event.alt()) result.bytes[result.length++] = '\x1b';
            result.bytes[result.length++] = event.control() ? control_character(ch) : ch;
            return result;
        }

        KeySequence escape_sequence(const char* sequence) {
            KeySequence result;
            if (sequence) {
                while (*sequence) result.bytes[result.length++] = *sequence++;
            }
            return result;
        }

        const char* navigation_sequence(Arch::ScanCode code) {
            switch (code) {
                case Arch::ScanCode::Up: return "\x1b[A";
                case Arch::ScanCode::Down: return "\x1b[B";
                case Arch::ScanCode::Right: return "\x1b[C";
                case Arch::ScanCode::Left: return "\x1b[D";
                case Arch::ScanCode::Home: return "\x1b[H";
                case Arch::ScanCode::End: return "\x1b[F";
                case Arch::ScanCode::Insert: return "\x1b[2~";
                case Arch::ScanCode::Delete: return "\x1b[3~";
                case Arch::ScanCode::PageUp: return "\x1b[5~";
                case Arch::ScanCode::PageDown: return "\x1b[6~";
                default: return nullptr;
            }
        }

        KeySequence translate_keypad(Arch::KeyEvent event) {
            constexpr char keypad[] = "789-456+1230.";
            constexpr const char* navigation[] = {
                "\x1b[H", "\x1b[A", "\x1b[5~", nullptr,
                "\x1b[D", nullptr, "\x1b[C", nullptr,
                "\x1b[F", "\x1b[B", "\x1b[6~", "\x1b[2~", "\x1b[3~"
            };
            const auto index = static_cast<unsigned>(event.code) - static_cast<unsigned>(Arch::ScanCode::Keypad7);
            const bool numeric = ((event.modifiers & Arch::NumLock) != 0) != event.shift();
            if (numeric || event.code == Arch::ScanCode::KeypadMinus || event.code == Arch::ScanCode::KeypadPlus) {
                return character_sequence(keypad[index], event);
            }
            return escape_sequence(navigation[index]);
        }
    }

    KeySequence translate_key(Arch::KeyEvent event) {
        const auto key = static_cast<unsigned>(event.code);
        if (key >= static_cast<unsigned>(Arch::ScanCode::Keypad7) &&
            key <= static_cast<unsigned>(Arch::ScanCode::KeypadPeriod)) {
            return translate_keypad(event);
        }
        if (const char ch = translate_character(event); ch != 0) {
            return character_sequence(ch, event);
        }
        return escape_sequence(navigation_sequence(event.code));
    }
}
