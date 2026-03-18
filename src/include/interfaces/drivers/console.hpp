#ifndef __INTERFACES__DRIVERS__CONSOLE_HPP
#define __INTERFACES__DRIVERS__CONSOLE_HPP 

#include <std/cstddef>

namespace Interfaces {
    void console_wirte(const char* buf, std::size_t len);
    void console_init();
}

#endif
