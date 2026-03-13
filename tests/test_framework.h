#ifndef HU_TEST_FRAMEWORK_H
#define HU_TEST_FRAMEWORK_H

#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int hu__total;
extern int hu__passed;
extern int hu__failed;
extern int hu__skipped;
extern int hu__suite_active;
extern const char *hu__suite_filter;
extern const char *hu__test_filter;
extern jmp_buf hu__jmp;

static inline const char *hu__strcasestr(const char *haystack, const char *needle) {
    if (!needle[0])
        return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n)
            return haystack;
    }
    return NULL;
}

#define HU_FAIL(...)                                    \
    do {                                                \
        printf("  FAIL  (%s:%d) ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                            \
        printf("\n");                                   \
        longjmp(hu__jmp, 1);                            \
    } while (0)

#define HU_ASSERT(cond)                          \
    do {                                         \
        if (!(cond))                             \
            HU_FAIL("assert failed: %s", #cond); \
    } while (0)

#define HU_ASSERT_EQ(a, b)                                               \
    do {                                                                 \
        long long _a = (long long)(a), _b = (long long)(b);              \
        if (_a != _b)                                                    \
            HU_FAIL("expected %lld == %lld (%s == %s)", _a, _b, #a, #b); \
    } while (0)

#define HU_ASSERT_NEQ(a, b)                                              \
    do {                                                                 \
        long long _a = (long long)(a), _b = (long long)(b);              \
        if (_a == _b)                                                    \
            HU_FAIL("expected %lld != %lld (%s != %s)", _a, _b, #a, #b); \
    } while (0)

#define HU_ASSERT_GT(a, b)                                             \
    do {                                                               \
        long long _a = (long long)(a), _b = (long long)(b);            \
        if (_a <= _b)                                                  \
            HU_FAIL("expected %lld > %lld (%s > %s)", _a, _b, #a, #b); \
    } while (0)

#define HU_ASSERT_STR_EQ(a, b)                                                            \
    do {                                                                                  \
        const char *_a = (a), *_b = (b);                                                  \
        if (!_a || !_b || strcmp(_a, _b) != 0)                                            \
            HU_FAIL("expected \"%s\" == \"%s\"", _a ? _a : "(null)", _b ? _b : "(null)"); \
    } while (0)

#define HU_ASSERT_NULL(a)                     \
    do {                                      \
        if ((a) != NULL)                      \
            HU_FAIL("expected NULL: %s", #a); \
    } while (0)

#define HU_ASSERT_NOT_NULL(a)                     \
    do {                                          \
        if ((a) == NULL)                          \
            HU_FAIL("expected not NULL: %s", #a); \
    } while (0)

#define HU_ASSERT_FLOAT_EQ(a, b, eps)              \
    do {                                           \
        double _a = (double)(a), _b = (double)(b); \
        if (fabs(_a - _b) > (eps))                 \
            HU_FAIL("expected %f ~= %f", _a, _b);  \
    } while (0)

#define HU_ASSERT_TRUE(a)  HU_ASSERT(a)
#define HU_ASSERT_FALSE(a) HU_ASSERT(!(a))

#define HU_RUN_TEST(fn)                                                 \
    do {                                                                \
        if (!hu__suite_active) {                                        \
            hu__skipped++;                                              \
            break;                                                      \
        }                                                               \
        if (hu__test_filter && !hu__strcasestr(#fn, hu__test_filter)) { \
            hu__skipped++;                                              \
            break;                                                      \
        }                                                               \
        hu__total++;                                                    \
        if (setjmp(hu__jmp) == 0) {                                     \
            fn();                                                       \
            hu__passed++;                                               \
            printf("  PASS  %s\n", #fn);                                \
        } else {                                                        \
            hu__failed++;                                               \
        }                                                               \
        fflush(stdout);                                                 \
    } while (0)

#define HU_TEST_SUITE(name)                                                                       \
    do {                                                                                          \
        hu__suite_active = (!hu__suite_filter || hu__strcasestr(name, hu__suite_filter) != NULL); \
        if (hu__suite_active)                                                                     \
            printf("\n=== %s ===\n", name);                                                       \
    } while (0)

#define HU_TEST_REPORT()                                              \
    do {                                                              \
        printf("\n--- Results: %d/%d passed", hu__passed, hu__total); \
        if (hu__failed > 0)                                           \
            printf(", %d FAILED", hu__failed);                        \
        if (hu__skipped > 0)                                          \
            printf(", %d skipped", hu__skipped);                      \
        printf(" ---\n");                                             \
    } while (0)

#define HU_TEST_EXIT() return hu__failed > 0 ? 1 : 0

#endif
