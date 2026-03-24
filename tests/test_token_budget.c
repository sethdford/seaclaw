#include "human/agent/token_budget.h"
#include "test_framework.h"
#include <string.h>

static void test_budget_init_defaults(void) {
    hu_token_budget_config_t config;
    hu_token_budget_init_defaults(&config);
    HU_ASSERT_FALSE(config.enabled);
    HU_ASSERT_EQ((int)config.spent, 0);
    HU_ASSERT_TRUE(config.tiers[HU_THINKING_TIER_REFLEXIVE].max_output_tokens > 0);
    HU_ASSERT_TRUE(config.tiers[HU_THINKING_TIER_RESEARCH].max_output_tokens >
                   config.tiers[HU_THINKING_TIER_REFLEXIVE].max_output_tokens);
}

static void test_budget_classify_reflexive(void) {
    HU_ASSERT_EQ((int)hu_token_budget_classify("hi", 2, 0, 0), (int)HU_THINKING_TIER_REFLEXIVE);
    HU_ASSERT_EQ((int)hu_token_budget_classify(NULL, 0, 0, 0), (int)HU_THINKING_TIER_REFLEXIVE);
}

static void test_budget_classify_analytical(void) {
    const char *prompt = "Please analyze and compare these two approaches in detail";
    HU_ASSERT_EQ((int)hu_token_budget_classify(prompt, strlen(prompt), 5, 1),
                 (int)HU_THINKING_TIER_ANALYTICAL);
}

static void test_budget_classify_research(void) {
    const char *prompt = "Please research this topic comprehensively";
    HU_ASSERT_EQ((int)hu_token_budget_classify(prompt, strlen(prompt), 0, 5),
                 (int)HU_THINKING_TIER_RESEARCH);
}

static void test_budget_classify_expert(void) {
    const char *prompt = "Implement and optimize the algorithm";
    HU_ASSERT_EQ((int)hu_token_budget_classify(prompt, strlen(prompt), 0, 4),
                 (int)HU_THINKING_TIER_EXPERT);
}

static void test_budget_classify_deep(void) {
    const char *prompt = "Please explain in detail step by step how this works";
    HU_ASSERT_EQ((int)hu_token_budget_classify(prompt, strlen(prompt), 0, 0),
                 (int)HU_THINKING_TIER_DEEP);
}

static void test_budget_get_tier(void) {
    hu_token_budget_config_t config;
    hu_token_budget_init_defaults(&config);
    const hu_tier_budget_t *tier = hu_token_budget_get(&config, HU_THINKING_TIER_DEEP);
    HU_ASSERT_NOT_NULL(tier);
    HU_ASSERT_TRUE(tier->max_input_tokens > 0);
    HU_ASSERT_TRUE(tier->thinking_budget > 0);
}

static void test_budget_can_spend(void) {
    hu_token_budget_config_t config;
    hu_token_budget_init_defaults(&config);
    config.enabled = true;
    config.total_budget = 1000;
    HU_ASSERT_TRUE(hu_token_budget_can_spend(&config, 500));
    config.spent = 800;
    HU_ASSERT_FALSE(hu_token_budget_can_spend(&config, 500));
}

static void test_budget_unlimited_always_can_spend(void) {
    hu_token_budget_config_t config;
    hu_token_budget_init_defaults(&config);
    config.enabled = true;
    config.total_budget = 0;
    HU_ASSERT_TRUE(hu_token_budget_can_spend(&config, 999999));
}

static void test_budget_record(void) {
    hu_token_budget_config_t config;
    hu_token_budget_init_defaults(&config);
    hu_token_budget_record(&config, 100);
    hu_token_budget_record(&config, 200);
    HU_ASSERT_EQ((int)config.spent, 300);
}

static void test_tier_names(void) {
    HU_ASSERT_STR_EQ(hu_thinking_tier_name(HU_THINKING_TIER_REFLEXIVE), "reflexive");
    HU_ASSERT_STR_EQ(hu_thinking_tier_name(HU_THINKING_TIER_CONVERSATIONAL), "conversational");
    HU_ASSERT_STR_EQ(hu_thinking_tier_name(HU_THINKING_TIER_ANALYTICAL), "analytical");
    HU_ASSERT_STR_EQ(hu_thinking_tier_name(HU_THINKING_TIER_DEEP), "deep");
    HU_ASSERT_STR_EQ(hu_thinking_tier_name(HU_THINKING_TIER_EXPERT), "expert");
    HU_ASSERT_STR_EQ(hu_thinking_tier_name(HU_THINKING_TIER_RESEARCH), "research");
}

static void test_budget_get_null(void) {
    HU_ASSERT_NULL(hu_token_budget_get(NULL, HU_THINKING_TIER_REFLEXIVE));
}

void run_token_budget_tests(void) {
    HU_TEST_SUITE("Token Budget");
    HU_RUN_TEST(test_budget_init_defaults);
    HU_RUN_TEST(test_budget_classify_reflexive);
    HU_RUN_TEST(test_budget_classify_analytical);
    HU_RUN_TEST(test_budget_classify_research);
    HU_RUN_TEST(test_budget_classify_expert);
    HU_RUN_TEST(test_budget_classify_deep);
    HU_RUN_TEST(test_budget_get_tier);
    HU_RUN_TEST(test_budget_can_spend);
    HU_RUN_TEST(test_budget_unlimited_always_can_spend);
    HU_RUN_TEST(test_budget_record);
    HU_RUN_TEST(test_tier_names);
    HU_RUN_TEST(test_budget_get_null);
}
