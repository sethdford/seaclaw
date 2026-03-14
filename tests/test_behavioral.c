#include "human/context/behavioral.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void double_text_default_config_low_closeness(void) {
    hu_double_text_config_t config = {
        .probability = 0.15,
        .min_gap_seconds = 30,
        .max_gap_seconds = 300,
        .only_close_friends = true,
    };
    bool result = hu_should_double_text(0.1, 1000, 2000, &config, 42);
    (void)result;
}

static void double_text_build_prompt_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_double_text_build_prompt(&alloc, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void double_text_high_closeness_may_trigger(void) {
    hu_double_text_config_t config = {
        .probability = 1.0,
        .min_gap_seconds = 1,
        .max_gap_seconds = 5,
        .only_close_friends = true,
    };
    bool result = hu_should_double_text(0.9, 100, 200, &config, 42);
    (void)result;
}

static void double_text_not_only_close_friends(void) {
    hu_double_text_config_t config = {
        .probability = 1.0,
        .min_gap_seconds = 1,
        .max_gap_seconds = 5,
        .only_close_friends = false,
    };
    bool result = hu_should_double_text(0.1, 100, 200, &config, 42);
    (void)result;
}

static void double_text_gap_too_small_returns_false(void) {
    hu_double_text_config_t config = {
        .probability = 1.0,
        .min_gap_seconds = 1000,
        .max_gap_seconds = 5000,
        .only_close_friends = false,
    };
    bool result = hu_should_double_text(0.9, 100, 101, &config, 42);
    HU_ASSERT_FALSE(result);
}

static void bookend_check_morning_close_friend(void) {
    hu_bookend_type_t bt = hu_bookend_check(8, true, false, 42);
    (void)bt;
}

static void bookend_check_already_sent_returns_none(void) {
    hu_bookend_type_t bt = hu_bookend_check(8, true, true, 42);
    HU_ASSERT_EQ(bt, HU_BOOKEND_NONE);
}

static void bookend_type_str_non_null(void) {
    HU_ASSERT_NOT_NULL(hu_bookend_type_str(HU_BOOKEND_NONE));
    HU_ASSERT_NOT_NULL(hu_bookend_type_str(HU_BOOKEND_MORNING));
    HU_ASSERT_NOT_NULL(hu_bookend_type_str(HU_BOOKEND_GOODNIGHT));
}

static void timezone_compute_valid_result(void) {
    hu_timezone_info_t tz = hu_timezone_compute(-5, 1700000000000ULL);
    HU_ASSERT_TRUE(tz.local_hour < 24);
}

void run_behavioral_tests(void) {
    HU_TEST_SUITE("behavioral");
    HU_RUN_TEST(double_text_default_config_low_closeness);
    HU_RUN_TEST(double_text_build_prompt_succeeds);
    HU_RUN_TEST(double_text_high_closeness_may_trigger);
    HU_RUN_TEST(double_text_not_only_close_friends);
    HU_RUN_TEST(double_text_gap_too_small_returns_false);
    HU_RUN_TEST(bookend_check_morning_close_friend);
    HU_RUN_TEST(bookend_check_already_sent_returns_none);
    HU_RUN_TEST(bookend_type_str_non_null);
    HU_RUN_TEST(timezone_compute_valid_result);
}
