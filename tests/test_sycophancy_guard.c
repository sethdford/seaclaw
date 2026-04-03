#include "test_framework.h"
#include "human/security/sycophancy_guard.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Detection ───────────────────────────────────────────────────── */

static void check_clean_response_not_flagged(void) {
    hu_sycophancy_result_t result = {0};
    const char *resp = "I see your point, but I think there are other angles to consider here.";
    HU_ASSERT(hu_sycophancy_check(resp, strlen(resp), "test", 4, 0.5f, &result) == HU_OK);
    HU_ASSERT(!result.flagged);
    HU_ASSERT(result.pattern_count == 0);
}

static void check_agreement_response_flagged(void) {
    hu_sycophancy_result_t result = {0};
    const char *resp = "You're absolutely right! I completely agree with you. "
                       "That's a great point and you nailed it!";
    HU_ASSERT(hu_sycophancy_check(resp, strlen(resp), "test", 4, 0.3f, &result) == HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.factor_scores[HU_SYCOPHANCY_UNCRITICAL_AGREEMENT] > 0.0f);
}

static void check_obsequious_response_scored(void) {
    hu_sycophancy_result_t result = {0};
    const char *resp = "What a wonderful insight! I'm so impressed by your brilliance!";
    HU_ASSERT(hu_sycophancy_check(resp, strlen(resp), "test", 4, 0.5f, &result) == HU_OK);
    HU_ASSERT(result.factor_scores[HU_SYCOPHANCY_OBSEQUIOUSNESS] > 0.0f);
}

static void check_excitement_response_scored(void) {
    hu_sycophancy_result_t result = {0};
    const char *resp = "That's so exciting! I love that! That sounds amazing!";
    HU_ASSERT(hu_sycophancy_check(resp, strlen(resp), "test", 4, 0.5f, &result) == HU_OK);
    HU_ASSERT(result.factor_scores[HU_SYCOPHANCY_EXCITEMENT] > 0.0f);
}

static void check_case_insensitive(void) {
    hu_sycophancy_result_t result = {0};
    const char *resp = "YOU'RE ABSOLUTELY RIGHT! I COMPLETELY AGREE!";
    HU_ASSERT(hu_sycophancy_check(resp, strlen(resp), "test", 4, 0.3f, &result) == HU_OK);
    HU_ASSERT(result.pattern_count >= 2);
}

static void check_null_fails(void) {
    hu_sycophancy_result_t result = {0};
    HU_ASSERT(hu_sycophancy_check(NULL, 0, "test", 4, 0.5f, &result) == HU_ERR_INVALID_ARGUMENT);
}

static void check_threshold_default(void) {
    hu_sycophancy_result_t result = {0};
    const char *resp = "Sure, sounds good.";
    HU_ASSERT(hu_sycophancy_check(resp, strlen(resp), "test", 4, 0.0f, &result) == HU_OK);
    HU_ASSERT(!result.flagged);
}

/* ── Friction directive ──────────────────────────────────────────── */

static void friction_not_flagged_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_sycophancy_result_t result = {0};
    result.flagged = false;
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_sycophancy_build_friction(&alloc, &result, "test", 4, &out, &out_len) == HU_OK);
    HU_ASSERT(out == NULL);
}

static void friction_flagged_returns_directive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_sycophancy_result_t result = {0};
    result.flagged = true;
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_sycophancy_build_friction(&alloc, &result, "test", 4, &out, &out_len) == HU_OK);
    HU_ASSERT(out != NULL);
    HU_ASSERT(strstr(out, "ANTI-SYCOPHANCY") != NULL);
    HU_ASSERT(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void friction_null_fails(void) {
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_sycophancy_build_friction(NULL, NULL, "t", 1, &out, &out_len) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Runner ──────────────────────────────────────────────────────── */

void run_sycophancy_guard_tests(void) {
    HU_TEST_SUITE("Sycophancy Guard");

    HU_RUN_TEST(check_clean_response_not_flagged);
    HU_RUN_TEST(check_agreement_response_flagged);
    HU_RUN_TEST(check_obsequious_response_scored);
    HU_RUN_TEST(check_excitement_response_scored);
    HU_RUN_TEST(check_case_insensitive);
    HU_RUN_TEST(check_null_fails);
    HU_RUN_TEST(check_threshold_default);

    HU_RUN_TEST(friction_not_flagged_returns_null);
    HU_RUN_TEST(friction_flagged_returns_directive);
    HU_RUN_TEST(friction_null_fails);
}
