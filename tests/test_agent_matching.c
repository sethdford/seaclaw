#include "human/agent/agent_profile.h"
#include "test_framework.h"
#include <string.h>

static void agent_matching_coding_specialist(void) {
    hu_agent_profile_t profile = {0};
    strncpy(profile.name, "coder", sizeof(profile.name) - 1);
    strncpy(profile.specialization, "coding", sizeof(profile.specialization) - 1);
    profile.success_rates[0] = 0.95; /* coding */
    profile.success_rates[3] = 0.5;   /* general */

    double score = 0.0;
    hu_error_t err = hu_agent_match_score(&profile, "coding", 6, &score);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(score >= 0.9);
}

static void agent_matching_generalist_fallback(void) {
    hu_agent_profile_t profile = {0};
    strncpy(profile.name, "generalist", sizeof(profile.name) - 1);
    strncpy(profile.specialization, "general", sizeof(profile.specialization) - 1);
    profile.success_rates[3] = 0.7;

    double score = 0.0;
    hu_error_t err = hu_agent_match_score(&profile, "unknown_category", 17, &score);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(score >= 0.5 && score <= 1.0);
}

static void agent_matching_null_args_returns_error(void) {
    hu_agent_profile_t profile = {0};
    strncpy(profile.specialization, "coding", sizeof(profile.specialization) - 1);
    double score = 0.0;

    HU_ASSERT_EQ(hu_agent_match_score(NULL, "coding", 6, &score), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_match_score(&profile, "coding", 6, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void agent_profile_update_success(void) {
    hu_agent_profile_t profile = {0};
    strncpy(profile.specialization, "coding", sizeof(profile.specialization) - 1);
    profile.success_rates[0] = 0.5;

    HU_ASSERT_EQ(hu_agent_profile_update(&profile, "coding", 6, true), HU_OK);
    HU_ASSERT_TRUE(profile.success_rates[0] > 0.5);
    HU_ASSERT_EQ(profile.task_count, 1);

    HU_ASSERT_EQ(hu_agent_profile_update(&profile, "coding", 6, false), HU_OK);
    HU_ASSERT_TRUE(profile.success_rates[0] < 0.6);
    HU_ASSERT_EQ(profile.task_count, 2);
}

static void agent_profile_update_unknown_category(void) {
    hu_agent_profile_t profile = {0};
    profile.success_rates[3] = 0.7;
    HU_ASSERT_EQ(hu_agent_profile_update(&profile, "unknown", 7, true), HU_OK);
    /* Falls back to general (index 3) */
    HU_ASSERT_TRUE(profile.success_rates[3] > 0.7);
}

static void agent_profile_update_null_args(void) {
    HU_ASSERT_EQ(hu_agent_profile_update(NULL, "x", 1, true), HU_ERR_INVALID_ARGUMENT);
}

void run_agent_matching_tests(void) {
    HU_TEST_SUITE("agent_matching");
    HU_RUN_TEST(agent_matching_coding_specialist);
    HU_RUN_TEST(agent_matching_generalist_fallback);
    HU_RUN_TEST(agent_matching_null_args_returns_error);
    HU_RUN_TEST(agent_profile_update_success);
    HU_RUN_TEST(agent_profile_update_unknown_category);
    HU_RUN_TEST(agent_profile_update_null_args);
}
