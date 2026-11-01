#pragma once

#include "arch/x86-64/arch/cpu.hpp"
#include "arch/x86-64/arch/arch_irq.hpp"
#include "atomic"

/* 
    Credit:
https://rigtorp.se/spinlock/
    */

namespace Util {
    struct SpinLock {
        std::atomic<bool> data;

        constexpr SpinLock() : data(false) {}

        SpinLock(SpinLock&) = delete;
        SpinLock(SpinLock&&) = delete;

        SpinLock operator=(SpinLock&) = delete;
        SpinLock operator=(SpinLock&&) = delete;

        void acquire() noexcept {
            while(data.exchange(true, std::memory_order_acquire));
        }

        void release() noexcept {
            data.store(false, std::memory_order_release);
        }

        // Lockable interface for std::lock_guard and std::unique_lock.
        void lock() noexcept { acquire(); }

        bool try_lock() noexcept {
            return !data.exchange(true, std::memory_order_acquire);
        }

        void unlock() noexcept { release(); }
    };

    struct IrqSpinLock {
        constexpr IrqSpinLock() = default; 
        
        IrqSpinLock(IrqSpinLock&) = delete;
        IrqSpinLock(IrqSpinLock&&) = delete;

        IrqSpinLock operator=(IrqSpinLock&) = delete;
        IrqSpinLock operator=(IrqSpinLock&&) = delete;

        void acquire() noexcept {
            std::uint64_t saved_flags;
            __asm volatile("pushfq; pop %0" : "=r"(saved_flags) :: "memory");
            // Mask interrupts before spinning to prevent local re-entry.
            Cpu::cli();
            inner.acquire();
            flags = saved_flags;
        }

        void release() noexcept {
            const auto saved_flags = flags;
            inner.release();
            if (saved_flags & (1ull << 9)) // IF Mask
            {
                Cpu::sti();
            }
        }

        private:
            uint64_t flags;
            SpinLock inner;
    };
}
