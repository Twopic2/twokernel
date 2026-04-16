#pragma once

#include "arch/x86-64/system/gdt.hpp"
#include "arch/x86-64/system/idt.hpp"
#include "arch/x86-64/system/irq.hpp"

namespace x86 {
    bool inital_init();
    void exception_load();
}
