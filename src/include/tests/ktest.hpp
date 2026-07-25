#pragma once

#include <util/kernel_logger.hpp>

/*
    Freestanding Catch2 look-alike.

        TEST_CASE("Pmm::alloc returns distinct pages", "[pmm]") {
            std::uint64_t pa = Memory::Pmm::alloc(1);
            REQUIRE(pa != 0);
            CHECK((pa & 0xFFF) == 0);
        }

    - Cases self-register through global ctors, so they exist after
      CxxRuntime::run_global_ctors() and before any KTest::run() call.
    - KTest::run("[pmm]") runs every case whose tag string contains the
      filter; KTest::run() runs everything registered.
    - Passing assertions are silent (like Catch2); failures print the file,
      line and stringified expression, plus a summary per run.
    - REQUIRE aborts the current case with `return`, so it is only valid
      directly inside the TEST_CASE body. CHECK records the failure and
      keeps going.
    - SECTION("...") only labels failures with the section name. Unlike real
      Catch2 it does NOT re-run the enclosing case once per section, so
      sections see the side effects of earlier sections.
*/

namespace KTest {
    struct TestCase {
        const char* name;
        const char* tags;
        void (*fn)();
        TestCase* next = nullptr;

        TestCase(const char* n, const char* t, void (*f)());
    };

    inline TestCase* first_test = nullptr;
    inline TestCase* last_test  = nullptr;

    inline TestCase::TestCase(const char* n, const char* t, void (*f)())
        : name(n), tags(t), fn(f) {
        if (last_test != nullptr) {
            last_test->next = this;
        } else {
            first_test = this;
        }
        last_test = this;
    }

    // Bookkeeping for the run/case currently executing.
    inline int assert_passed = 0;
    inline int assert_failed = 0;

    inline const char* current_name    = nullptr;
    inline const char* current_section = nullptr;
    inline bool current_failed = false;
    inline bool header_printed = false;

    inline bool contains(const char* haystack, const char* needle) {
        if (needle == nullptr || *needle == '\0') {
            return true;
        }
        if (haystack == nullptr) {
            return false;
        }
        for (; *haystack != '\0'; haystack++) {
            const char* h = haystack;
            const char* n = needle;
            while (*h != '\0' && *n != '\0' && *h == *n) {
                h++;
                n++;
            }
            if (*n == '\0') {
                return true;
            }
        }
        return false;
    }

    inline void print_case_header() {
        if (header_printed) {
            return;
        }
        header_printed = true;
        Util::klog("-------------------------------------------------------------------------------\n");
        Util::klog("%s\n", current_name);
        Util::klog("-------------------------------------------------------------------------------\n");
    }

    inline void on_pass() {
        assert_passed++;
    }

    inline void on_fail(const char* macro, const char* expr, const char* file, int line) {
        assert_failed++;
        current_failed = true;
        print_case_header();
        Util::klog("%s:%d: FAILED:\n", file, line);
        Util::klog("  %s( %s )\n", macro, expr);
        if (current_section != nullptr) {
            Util::klog("  in SECTION( \"%s\" )\n", current_section);
        }
    }

    struct SectionGuard {
        const char* prev;

        explicit SectionGuard(const char* name) : prev(current_section) {
            current_section = name;
        }
        ~SectionGuard() {
            current_section = prev;
        }
        explicit operator bool() const {
            return true;
        }
    };

    inline void run(const char* tag_filter = nullptr) {
        assert_passed = 0;
        assert_failed = 0;

        int cases_passed = 0;
        int cases_failed = 0;

        if (tag_filter != nullptr) {
            Util::klog("Filters: %s\n", tag_filter);
        }

        for (TestCase* tc = first_test; tc != nullptr; tc = tc->next) {
            if (tag_filter != nullptr && !contains(tc->tags, tag_filter)) {
                continue;
            }

            current_name    = tc->name;
            current_section = nullptr;
            current_failed  = false;
            header_printed  = false;

            tc->fn();

            if (current_failed) {
                cases_failed++;
            } else {
                cases_passed++;
            }
        }

        Util::klog("===============================================================================\n");
        if (cases_failed == 0) {
            Util::klog("All tests passed (%d assertions in %d test cases)\n",
                       assert_passed, cases_passed);
        } else {
            Util::klog("test cases: %d | %d passed | %d failed\n",
                       cases_passed + cases_failed, cases_passed, cases_failed);
            Util::klog("assertions: %d | %d passed | %d failed\n",
                       assert_passed + assert_failed, assert_passed, assert_failed);
        }
    }
}

#define KTEST_CONCAT2(a, b) a##b
#define KTEST_CONCAT(a, b) KTEST_CONCAT2(a, b)

#define TEST_CASE(test_name, test_tags)                                        \
    static void KTEST_CONCAT(ktest_func_, __LINE__)();                         \
    static ::KTest::TestCase KTEST_CONCAT(ktest_case_, __LINE__){               \
        test_name, test_tags, &KTEST_CONCAT(ktest_func_, __LINE__)             \
    };                                                                          \
    static void KTEST_CONCAT(ktest_func_, __LINE__)()

#define KTEST_ASSERT_IMPL(macro_name, expr, fail_action)                       \
    do {                                                                        \
        if (static_cast<bool>(expr)) {                                          \
            ::KTest::on_pass();                                                 \
        } else {                                                                \
            ::KTest::on_fail(macro_name, #expr, __FILE__, __LINE__);            \
            fail_action;                                                        \
        }                                                                       \
    } while (0)

#define REQUIRE(expr)       KTEST_ASSERT_IMPL("REQUIRE", expr, return)
#define CHECK(expr)         KTEST_ASSERT_IMPL("CHECK", expr, (void)0)
#define REQUIRE_FALSE(expr) KTEST_ASSERT_IMPL("REQUIRE_FALSE", !(expr), return)
#define CHECK_FALSE(expr)   KTEST_ASSERT_IMPL("CHECK_FALSE", !(expr), (void)0)

#define SECTION(section_name)                                                   \
    if (::KTest::SectionGuard KTEST_CONCAT(ktest_section_, __LINE__){ section_name })

namespace Tests {
    // Not a test case: spawns the two demo threads from schedule_tests.cpp.
    void add_init_thread();
}
