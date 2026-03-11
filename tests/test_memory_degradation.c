#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/memory/degradation.h"
#include "test_framework.h"
#include <string.h>

static void test_memory_degradation_protected_promised_mindy(void) {
    const char *s = "I promised Mindy";
    HU_ASSERT_TRUE(hu_memory_degradation_is_protected(s, strlen(s)));

    hu_allocator_t alloc = hu_system_allocator();
    size_t out_len = 0;
    char *r = hu_memory_degradation_apply(&alloc, s, strlen(s), 99, 0.10f, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_STR_EQ(r, s);
    HU_ASSERT_EQ(out_len, strlen(s));
    hu_str_free(&alloc, r);
}

static void test_memory_degradation_rate_zero_never_degrade(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "We met on Tuesday at the cafe";
    size_t len = strlen(content);

    for (uint32_t seed = 0; seed < 100; seed++) {
        size_t out_len = 0;
        char *r = hu_memory_degradation_apply(&alloc, content, len, seed, 0.0f, &out_len);
        HU_ASSERT_NOT_NULL(r);
        HU_ASSERT_STR_EQ(r, content);
        HU_ASSERT_EQ(out_len, len);
        hu_str_free(&alloc, r);
    }
}

static void test_memory_degradation_seed_fuzz_day_changed(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "We met on Tuesday";
    size_t len = strlen(content);

    /* Seed 90 gives roll 0.90, which is fuzz range (0.90-0.95) with rate 0.10 */
    size_t out_len = 0;
    char *r = hu_memory_degradation_apply(&alloc, content, len, 90, 0.10f, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_TRUE(strstr(r, "Wednesday") != NULL || strstr(r, "Monday") != NULL);
    HU_ASSERT_TRUE(strstr(r, "Tuesday") == NULL);
    hu_str_free(&alloc, r);
}

static void test_memory_degradation_seed_forget_remind_me(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "We met on Tuesday at the cafe";
    size_t len = strlen(content);

    /* Seed 95 gives roll 0.95, which is remind path (0.95-1.0) */
    size_t out_len = 0;
    char *r = hu_memory_degradation_apply(&alloc, content, len, 95, 0.10f, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_TRUE(strstr(r, "remind me") != NULL);
    HU_ASSERT_TRUE(strstr(r, "what was that about") != NULL);
    hu_str_free(&alloc, r);
}

void run_memory_degradation_tests(void) {
    HU_TEST_SUITE("memory_degradation");
    HU_RUN_TEST(test_memory_degradation_protected_promised_mindy);
    HU_RUN_TEST(test_memory_degradation_rate_zero_never_degrade);
    HU_RUN_TEST(test_memory_degradation_seed_fuzz_day_changed);
    HU_RUN_TEST(test_memory_degradation_seed_forget_remind_me);
}
