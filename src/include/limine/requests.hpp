#ifndef __LIMINE__REQUESTS_HPP
#define __LIMINE__REQUESTS_HPP 

#include <limine.h>

namespace Limine {
    extern volatile limine_framebuffer_request framebuffer_request;
}

namespace Limine {
    void base_revision_check();
} 

#endif