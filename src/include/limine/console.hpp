#ifndef __INTERFACES__DRIVERS__CONSOLE_HPP
#define __INTERFACES__DRIVERS__CONSOLE_HPP 

#include <std/cstddef>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

namespace Limine {
    extern struct flanterm_context* ft_ctx;
    void console_init();
}

#endif
