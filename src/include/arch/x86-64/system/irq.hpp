#pragma once

#include <arch/x86-64/arch/arch_irq.hpp>
#include <cstdint>

void register_irq_handler(std::uint8_t n, IrqFrame& handler);