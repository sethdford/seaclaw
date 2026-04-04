#include "human/core/allocator.h"
#include "human/security/sycophancy_guard.h"
#include "test_framework.h"
#include <string.h>

static void sycophancy_neutral_response_not_flagged(void) {
    const char *response = "The capital of France is Paris.";
    const char *user = "What is the capital of France?";
    hu_sycophancy_result_t r;
    memset(&r, 0, sizeof(r));
    HU_ASSERT_EQ(hu_sycophancy_check(response, strlen(response), user, strlen(user), 0.5f, &r),
                 HU_OK);
    HU_ASSERT_FALSE(r.flagged);
}

static void sycophancy_obsequious_response_flagged(void) {
    const char *response =
        "You're absolutely right. I completely agree. Exactly right. "
        "What a wonderful point. You're so talented. Brilliant insight.";
    const char *user = "Here is my question.";
    hu_sycophancy_result_t r;
    memset(&r, 0, sizeof(r));
    HU_ASSERT_EQ(hu_sycophancy_check(response, strlen(response), user, strlen(user), 0.5f, &r),
                 HU_OK);
    HU_ASSERT_TRUE(r.flagged);
}

static void sycophancy_check_null_response_returns_error(void) {
    hu_sycophancy_result_t r;
    memset(&r, 0, sizeof(r));
    HU_ASSERT_EQ(hu_sycophancy_check(NULL, 0U, "hi", 2U, 0.5f, &r), HU_ERR_INVALID_ARGUMENT);
}

static void sycophancy_threshold_affects_flagging(void) {
    const char *response =
        "You're absolutely right. I completely agree. Exactly right.";
    const char *user = "x";
    hu_sycophancy_result_t low;
    hu_sycophancy_result_t high;
    memset(&low, 0, sizeof(low));
    memset(&high, 0, sizeof(high));
    HU_ASSERT_EQ(hu_sycophancy_check(response, strlen(response), user, strlen(user), 0.01f, &low),
                 HU_OK);
    HU_ASSERT_EQ(
        hu_sycophancy_check(response, strlen(response), user, strlen(user), 0.99f, &high), HU_OK);
    HU_ASSERT_TRUE(low.flagged);
    HU_ASSERT_FALSE(high.flagged);
}

static void sycophancy_build_friction_produces_string(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *response =
        "You're absolutely right. I completely agree. Exactly right.";
    const char *user = "I think we should do this.";
    hu_sycophancy_result_t r;
    memset(&r, 0, sizeof(r));
    HU_ASSERT_EQ(hu_sycophancy_check(response, strlen(response), user, strlen(user), 0.5f, &r),
                 HU_OK);
    HU_ASSERT_TRUE(r.flagged);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_sycophancy_build_friction(&alloc, &r, user, strlen(user), &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_GT((long long)out_len, 0LL);
    HU_ASSERT_STR_CONTAINS(out, "ANTI-SYCOPHANCY");
    alloc.free(alloc.ctx, out, out_len + 1U);
}

static void sycophancy_build_friction_null_result_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0U;
    HU_ASSERT_EQ(hu_sycophancy_build_friction(&alloc, NULL, "hi", 2U, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_sycophancy_guard_tests(void) {
    HU_TEST_SUITE("sycophancy_guard");
    HU_RUN_TEST(sycophancy_neutral_response_not_flagged);
    HU_RUN_TEST(sycophancy_obsequious_response_flagged);
    HU_RUN_TEST(sycophancy_check_null_response_returns_error);
    HU_RUN_TEST(sycophancy_threshold_affects_flagging);
    HU_RUN_TEST(sycophancy_build_friction_produces_string);
    HU_RUN_TEST(sycophancy_build_friction_null_result_returns_error);
}
