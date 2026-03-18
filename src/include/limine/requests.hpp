#ifndef __LIMINE__REQUESTS_HPP
#define __LIMINE__REQUESTS_HPP 

#include "limine.h"

namespace limine {
    extern volatile limine_framebuffer_request framebuffer_request;
    extern volatile limine_memmap_request      memmap_request;
} 

#endif