#include "test_framework.h"
#include "human/agent/process_reward.h"
#include <math.h>
#include <string.h>

static void prm_score_empty_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, "", 0, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void prm_heuristic_high_score_for_specific_steps(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "Step 1: The answer is 42 because x=42.\n\nStep 2: Therefore the result is confirmed.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT_EQ((int)result.step_count, 2);
    HU_ASSERT(result.steps[0].score > 0.5);
    HU_ASSERT(result.steps[1].score > 0.5);
    HU_ASSERT(result.aggregate_score > 0.5);
    hu_prm_result_free(&alloc, &result);
}

static void prm_heuristic_low_score_for_hedging(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "Maybe the answer is something. I think it might work perhaps.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT(result.steps[0].score < 0.5);
    hu_prm_result_free(&alloc, &result);
}

static void prm_contradiction_detection(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "The answer is definitely 42 because of the formula.\n\n"
                    "However maybe the answer might not be right perhaps.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT(result.steps[0].score > result.steps[1].score);
    hu_prm_result_free(&alloc, &result);
}

static void prm_chain_valid_when_all_above_threshold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "The result is 42 because of the calculation.\n\nTherefore we conclude with the value 42.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT(result.chain_valid);
    hu_prm_result_free(&alloc, &result);
}

static void prm_chain_invalid_when_step_below_threshold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    cfg.correctness_threshold = 0.8;
    hu_prm_result_t result;
    const char *r = "Maybe perhaps I think it might work.\n\nBut not sure really.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT(!result.chain_valid);
    hu_prm_result_free(&alloc, &result);
}

static void prm_aggregate_is_geometric_mean(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "First step with number 42.\n\nSecond step with number 99.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    double expected = pow(result.steps[0].score * result.steps[1].score, 0.5);
    HU_ASSERT(fabs(result.aggregate_score - expected) < 0.001);
    hu_prm_result_free(&alloc, &result);
}

static void prm_step_splitting_numbered_lists(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "1. First step\n2. Second step\n3. Third step";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT_EQ((int)result.step_count, 3);
    hu_prm_result_free(&alloc, &result);
}

static void prm_step_splitting_paragraphs(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "Para one content.\n\nPara two content.\n\nPara three content.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT_EQ((int)result.step_count, 3);
    hu_prm_result_free(&alloc, &result);
}

static void prm_mcts_integration_uses_prm_score(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "The calculation yields 100 because we summed 50+50.";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT(result.aggregate_score > 0.0);
    hu_prm_result_free(&alloc, &result);
}

static void prm_llm_path_under_test_uses_heuristic(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    hu_prm_result_t result;
    const char *r = "A clear step with specific data [1].";
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, &cfg, r, strlen(r), &result), HU_OK);
    HU_ASSERT(result.step_count == 1);
    hu_prm_result_free(&alloc, &result);
}

static void prm_null_config_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_result_t result;
    HU_ASSERT_EQ(hu_prm_score_chain(&alloc, NULL, "test", 4, &result), HU_ERR_INVALID_ARGUMENT);
}

static void prm_config_default_values(void) {
    hu_prm_config_t cfg = hu_prm_config_default();
    HU_ASSERT(cfg.enabled);
    HU_ASSERT(cfg.provider == NULL);
    HU_ASSERT(cfg.model == NULL);
    HU_ASSERT(fabs(cfg.correctness_threshold - 0.5) < 0.001);
}

static void prm_free_handles_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_result_t result;
    memset(&result, 0, sizeof(result));
    hu_prm_result_free(&alloc, &result);
    hu_prm_result_free(&alloc, NULL);
    hu_prm_result_free(NULL, &result);
}

static void prm_score_step_with_context(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();
    double score = 0.0;
    const char *step = "Therefore the answer is 42 because of the equation.";
    HU_ASSERT_EQ(hu_prm_score_step(&alloc, &cfg, step, strlen(step), "context", 7, &score), HU_OK);
    HU_ASSERT(score > 0.5);
}

void run_process_reward_tests(void) {
    HU_TEST_SUITE("Process Reward Model");
    HU_RUN_TEST(prm_score_empty_returns_error);
    HU_RUN_TEST(prm_heuristic_high_score_for_specific_steps);
    HU_RUN_TEST(prm_heuristic_low_score_for_hedging);
    HU_RUN_TEST(prm_contradiction_detection);
    HU_RUN_TEST(prm_chain_valid_when_all_above_threshold);
    HU_RUN_TEST(prm_chain_invalid_when_step_below_threshold);
    HU_RUN_TEST(prm_aggregate_is_geometric_mean);
    HU_RUN_TEST(prm_step_splitting_numbered_lists);
    HU_RUN_TEST(prm_step_splitting_paragraphs);
    HU_RUN_TEST(prm_mcts_integration_uses_prm_score);
    HU_RUN_TEST(prm_llm_path_under_test_uses_heuristic);
    HU_RUN_TEST(prm_null_config_returns_error);
    HU_RUN_TEST(prm_config_default_values);
    HU_RUN_TEST(prm_free_handles_null);
    HU_RUN_TEST(prm_score_step_with_context);
}
