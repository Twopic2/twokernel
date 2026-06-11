#pragma once

#include <util/kernel_logger.hpp>
#include <cstdint>

namespace KTest {
    inline int passed = 0;
    inline int failed = 0;

    inline void reset() { passed = 0; failed = 0; }

    inline void summary(const char* suite) {
        Util::klog("[%s] %d passed, %d failed\n", suite, passed, failed);
    }
}

#define KTEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            Util::klog("  [PASS] %s\n", msg); \
            KTest::passed++; \
        } else { \
            Util::klog("  [FAIL] %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
            KTest::failed++; \
        } \
    } while (0)

namespace Tests {
    void run_pmm_tests();
    void run_vmm_tests();
    void run_irq_tests();
    void run_scheduler_tests();
    void add_init_thread(); 
    void deque_tests();
    void keyboard_buffer_tests();
    void keyboard_irq_tests();

    inline void run_all() {
        run_pmm_tests();
        run_vmm_tests();
        run_irq_tests();
        run_scheduler_tests();
        Util::klog("=== Total: %d passed, %d failed ===\n", KTest::passed, KTest::failed);
    }
}
