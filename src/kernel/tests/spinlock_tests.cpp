#include <tests/ktest.hpp>
#include <util/spinlocks.hpp>
#include <util/irq_gaurd.hpp>
#include <arch/x86-64/arch/cpu.hpp>
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace Tests {
    namespace {
        std::uint64_t read_rflags() {
            std::uint64_t flags;
            asm volatile("pushfq; pop %0" : "=r"(flags) :: "memory");
            return flags;
        }

        bool irqs_enabled() {
            return (read_rflags() & Util::IF) != 0;
        }

        // Every IrqSpinLock case ends with the cpu's IF in whatever state the
        // lock left it, and a failing REQUIRE returns out of the case with
        // interrupts still off. Put IF back on the way out so one broken case
        // doesn't silently run every later test (spinlock or otherwise) with
        // interrupts disabled.
        struct IrqStateFixture {
            bool was_enabled = irqs_enabled();

            ~IrqStateFixture() {
                if (was_enabled) {
                    Cpu::sti();
                } else {
                    Cpu::cli();
                }
            }
        };

        // acquire() spins until it wins, so a held lock can't be acquired a
        // second time from the one cpu running these tests -- the call would
        // never return. This is exactly the exchange acquire() loops on:
        // false back means the lock was free and we now hold it, true means
        // acquire() would have spun right here.
        bool try_acquire(Util::SpinLock& lock) {
            return !lock.data.exchange(true, std::memory_order_acquire);
        }

        // A kernel-lifetime lock has to land in .bss already unlocked: if this
        // needed a global ctor, anything taking the lock before
        // run_global_ctors() would be reading uninitialised memory. constinit
        // makes that a compile error rather than a boot-time mystery.
        constinit Util::SpinLock g_static_lock;
    }

    TEST_CASE("a default-constructed SpinLock is unlocked", "[spinlock]") {
        Util::SpinLock lock {};
        CHECK_FALSE(lock.data.load(std::memory_order_relaxed));
    }

    TEST_CASE("a constant-initialised SpinLock starts unlocked too", "[spinlock]") {
        // No global ctor ran for this one -- it is zeroed .bss.
        CHECK_FALSE(g_static_lock.data.load(std::memory_order_relaxed));

        g_static_lock.acquire();
        CHECK(g_static_lock.data.load(std::memory_order_relaxed));
        g_static_lock.release();
        CHECK_FALSE(g_static_lock.data.load(std::memory_order_relaxed));
    }

    TEST_CASE("acquire() takes the lock and release() gives it back", "[spinlock]") {
        Util::SpinLock lock {};

        lock.acquire();
        CHECK(lock.data.load(std::memory_order_relaxed));

        lock.release();
        CHECK_FALSE(lock.data.load(std::memory_order_relaxed));
    }

    TEST_CASE("acquire() on a free lock does not spin", "[spinlock]") {
        Util::SpinLock lock {};

        // The very first exchange sees false, so the while loop body never
        // runs and acquire() returns immediately.
        CHECK(try_acquire(lock));
        lock.release();
    }

    TEST_CASE("a held lock turns the next acquire() into a spin", "[spinlock]") {
        Util::SpinLock lock {};

        lock.acquire();
        // A second acquirer's exchange comes back true -- that is the spin.
        // Calling acquire() here instead would hang the kernel, which is the
        // whole point: this is what mutual exclusion looks like from outside.
        CHECK_FALSE(try_acquire(lock));
        // The failed attempt wrote true over true, so the lock is untouched
        // and still ours to release.
        CHECK(lock.data.load(std::memory_order_relaxed));

        lock.release();
        CHECK_FALSE(lock.data.load(std::memory_order_relaxed));
    }

    TEST_CASE("release() lets the next acquirer straight through", "[spinlock]") {
        Util::SpinLock lock {};

        lock.acquire();
        REQUIRE_FALSE(try_acquire(lock));

        lock.release();
        CHECK(try_acquire(lock));      // the waiter would win on its next pass
        lock.release();
    }

    TEST_CASE("the lock survives repeated acquire/release cycles", "[spinlock]") {
        Util::SpinLock lock {};

        bool held_every_time = true;
        bool free_every_time = true;

        for (int i = 0; i < 1000; ++i) {
            lock.acquire();
            if (!lock.data.load(std::memory_order_relaxed)) {
                held_every_time = false;
            }
            lock.release();
            if (lock.data.load(std::memory_order_relaxed)) {
                free_every_time = false;
            }
        }

        CHECK(held_every_time);
        CHECK(free_every_time);
        CHECK_FALSE(lock.data.load(std::memory_order_relaxed));
    }

    TEST_CASE("a double release leaves the lock free, not negative", "[spinlock]") {
        // The flag is a bool, not a count, so an unbalanced release is a
        // no-op rather than something that wraps around.
        Util::SpinLock lock {};

        lock.acquire();
        lock.release();
        lock.release();

        CHECK_FALSE(lock.data.load(std::memory_order_relaxed));
        CHECK(try_acquire(lock));
        lock.release();
    }

    TEST_CASE("distinct SpinLocks protect distinct things", "[spinlock]") {
        Util::SpinLock a {};
        Util::SpinLock b {};

        a.acquire();
        CHECK(a.data.load(std::memory_order_relaxed));
        CHECK_FALSE(b.data.load(std::memory_order_relaxed));

        // Holding a says nothing about b.
        CHECK(try_acquire(b));

        a.release();
        b.release();
        CHECK_FALSE(a.data.load(std::memory_order_relaxed));
        CHECK_FALSE(b.data.load(std::memory_order_relaxed));
    }

    TEST_CASE("a SpinLock cannot be copied or moved", "[spinlock]") {
        // A copied lock would hand out a second, independent flag for the
        // same critical section.
        static_assert(!std::is_copy_constructible_v<Util::SpinLock>);
        static_assert(!std::is_move_constructible_v<Util::SpinLock>);
        static_assert(!std::is_copy_assignable_v<Util::SpinLock>);
        static_assert(!std::is_move_assignable_v<Util::SpinLock>);

        CHECK_FALSE(std::is_copy_constructible_v<Util::SpinLock>);
        CHECK_FALSE(std::is_move_constructible_v<Util::SpinLock>);
        CHECK_FALSE(std::is_copy_assignable_v<Util::SpinLock>);
        CHECK_FALSE(std::is_move_assignable_v<Util::SpinLock>);
    }

    TEST_CASE("the lock word is lock free", "[spinlock]") {
        // A std::atomic<bool> that fell back to a library mutex would be
        // calling into something that doesn't exist here.
        static_assert(std::atomic<bool>::is_always_lock_free);
        CHECK(std::atomic<bool>::is_always_lock_free);
        CHECK(sizeof(Util::SpinLock) <= sizeof(std::uint64_t));
    }

    TEST_CASE("an IrqSpinLock cannot be copied or moved", "[spinlock]") {
        static_assert(!std::is_copy_constructible_v<Util::IrqSpinLock>);
        static_assert(!std::is_move_constructible_v<Util::IrqSpinLock>);
        static_assert(!std::is_copy_assignable_v<Util::IrqSpinLock>);
        static_assert(!std::is_move_assignable_v<Util::IrqSpinLock>);

        CHECK_FALSE(std::is_copy_constructible_v<Util::IrqSpinLock>);
        CHECK_FALSE(std::is_move_constructible_v<Util::IrqSpinLock>);
    }

    // The whole reason for the irq-aware variant: a handler that runs on this
    // cpu and wants the same lock would spin against a holder that can never
    // make progress, so the holder runs with interrupts off.
    TEST_CASE("acquire() disables interrupts and release() puts them back", "[spinlock]") {
        IrqStateFixture fixture {};

        Cpu::sti();
        REQUIRE(irqs_enabled());

        Util::IrqSpinLock lock {};      // captures IF = 1

        lock.acquire();
        CHECK_FALSE(irqs_enabled());

        lock.release();
        CHECK(irqs_enabled());
    }

    // The mirror case: a caller that already had interrupts off must get them
    // back off, not silently switched on underneath it.
    TEST_CASE("release() leaves interrupts off when they were off at acquire", "[spinlock]") {
        IrqStateFixture fixture {};

        Cpu::cli();
        REQUIRE_FALSE(irqs_enabled());

        Util::IrqSpinLock lock {};      // captures IF = 0

        lock.acquire();
        CHECK_FALSE(irqs_enabled());

        lock.release();
        CHECK_FALSE(irqs_enabled());
    }

    TEST_CASE("an IrqSpinLock can be taken and dropped over and over", "[spinlock]") {
        IrqStateFixture fixture {};

        Cpu::sti();
        REQUIRE(irqs_enabled());

        Util::IrqSpinLock lock {};

        bool off_while_held = true;
        bool on_while_free  = true;

        for (int i = 0; i < 100; ++i) {
            lock.acquire();
            if (irqs_enabled()) {
                off_while_held = false;
            }
            lock.release();
            if (!irqs_enabled()) {
                on_while_free = false;
            }
        }

        CHECK(off_while_held);
        CHECK(on_while_free);
    }

    // ---------------------------------------------------------------------
    // The two cases below are why the pushfq lives in acquire() rather than
    // in the constructor: the saved RFLAGS has to be the interrupt state at
    // the moment the lock was taken, not the one it happened to be built
    // with. Both of these fail if that save moves back into the ctor.
    // ---------------------------------------------------------------------

    // Every kernel-lifetime lock is constructed during early boot, with
    // interrupts still off. Saving in the ctor would pin its IF to 0 forever,
    // so the first release() after sti() would drop interrupts on the floor
    // and never turn them back on.
    TEST_CASE("the saved IF comes from acquire(), not from construction", "[spinlock]") {
        IrqStateFixture fixture {};

        Cpu::cli();
        Util::IrqSpinLock lock {};      // built during early boot: IF = 0

        Cpu::sti();                     // ... interrupts come up later
        REQUIRE(irqs_enabled());

        lock.acquire();
        CHECK_FALSE(irqs_enabled());

        lock.release();
        CHECK(irqs_enabled());          // the caller had IF = 1, so it gets it back
    }

    // Nesting two locks: the inner release() must restore the state that the
    // inner acquire() found, which is "off" because the outer one is still
    // held. Re-enabling here exposes the outer critical section to exactly
    // the handler it was trying to keep out.
    TEST_CASE("releasing an inner lock does not re-enable interrupts", "[spinlock]") {
        IrqStateFixture fixture {};

        Cpu::sti();
        REQUIRE(irqs_enabled());

        Util::IrqSpinLock outer {};
        Util::IrqSpinLock inner {};

        outer.acquire();
        REQUIRE_FALSE(irqs_enabled());

        inner.acquire();
        CHECK_FALSE(irqs_enabled());

        inner.release();
        CHECK_FALSE(irqs_enabled());    // outer still holds: stay masked

        outer.release();
        CHECK(irqs_enabled());          // outermost release hands IF back
    }
}
