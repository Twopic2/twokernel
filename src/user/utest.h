#ifndef USER_UTEST_H
#define USER_UTEST_H

/*
    ? Minimal userspace test harness.

    ? There is no write() yet, so the only channel back to the kernel is the
    ? exit status. A program runs its checks and exits with 0 when they all
    ? pass, or with the 1-based index of the first check that failed. The
    ? kernel's exit handler prints that number.

    ? Once write(1, ...) lands, UT_CHECK can print the failing expression and
    ? this degrades into a normal assert.
*/

static int utest_index;
static int utest_failed;

#define UT_BEGIN()                                                             \
    do {                                                                       \
        utest_index = 0;                                                       \
        utest_failed = 0;                                                      \
    } while (0)

/* Numbering counts every check, so the exit status names the failing line
   even when an earlier check already failed. */
#define UT_CHECK(expr)                                                         \
    do {                                                                       \
        utest_index++;                                                         \
        if (!utest_failed && !(expr)) {                                        \
            utest_failed = utest_index;                                        \
        }                                                                      \
    } while (0)

#define UT_RESULT() (utest_failed)

#endif
