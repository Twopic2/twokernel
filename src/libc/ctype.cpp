#include <libc/ctype.h>

namespace LibC {
    extern "C" {
        int isdigit(int c)  { return c >= '0' && c <= '9'; }
        int isupper(int c)  { return c >= 'A' && c <= 'Z'; }
        int islower(int c)  { return c >= 'a' && c <= 'z'; }
        int isalpha(int c)  { return isupper(c) || islower(c); }
        int isalnum(int c)  { return isalpha(c) || isdigit(c); }
        int isspace(int c)  { return c == ' ' || (c >= '\t' && c <= '\r'); }
        int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
        int iscntrl(int c)  { return (c >= 0 && c < 0x20) || c == 0x7f; }
        int isprint(int c)  { return c >= 0x20 && c < 0x7f; }
        int isgraph(int c)  { return c > 0x20 && c < 0x7f; }
        int ispunct(int c)  { return isprint(c) && !isalnum(c) && c != ' '; }
        int tolower(int c)  { return isupper(c) ? c + ('a' - 'A') : c; }
        int toupper(int c)  { return islower(c) ? c - ('a' - 'A') : c; }
    }
}
