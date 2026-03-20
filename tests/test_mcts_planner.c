#include "test_framework.h"
#include "human/agent/mcts_planner.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <string.h>

static void mcts_config_default_values(void) {
    hu_mcts_config_t c = hu_mcts_config_default();
    HU_ASSERT_EQ(c.max_iterations, 100);
    HU_ASSERT_EQ(c.max_depth, 5);
    HU_ASSERT_TRUE(c.exploration_c > 1.0 && c.exploration_c < 2.0);
    HU_ASSERT_EQ(c.max_time_ms, 5000);
    HU_ASSERT_EQ(c.max_llm_calls, 20);
}

static void mcts_plan_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_result_t result;
    memset(&result, 0, sizeof(result));
    const char *goal = "achieve X";
    const char *ctx = "context";

    HU_ASSERT_EQ(hu_mcts_plan(NULL, goal, 9, ctx, 7, NULL, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, NULL, 9, ctx, 7, NULL, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_mcts_plan(&alloc, goal, 9, ctx, 7, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void mcts_plan_finds_best_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t config = hu_mcts_config_default();
    config.max_iterations = 20;
    hu_mcts_result_t result;
    memset(&result, 0, sizeof(result));

    const char *goal = "complete task";
    const char *ctx = "some context";

    HU_ASSERT_EQ(hu_mcts_plan(&alloc, goal, strlen(goal), ctx, strlen(ctx),
                              &config, &result), HU_OK);

    HU_ASSERT_TRUE(result.best_action_len > 0);
    HU_ASSERT_TRUE(result.best_action[0] != '\0');
    HU_ASSERT_TRUE(result.best_value > 0.0);
    HU_ASSERT_TRUE(result.action_count > 0);
    HU_ASSERT_NOT_NULL(result.actions);
    HU_ASSERT_NOT_NULL(result.action_lens);
    hu_mcts_result_free_path(&alloc, &result);
}

static void mcts_plan_respects_max_iterations(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t config = hu_mcts_config_default();
    config.max_iterations = 10;
    hu_mcts_result_t result;
    memset(&result, 0, sizeof(result));

    const char *goal = "goal";
    const char *ctx = "ctx";

    HU_ASSERT_EQ(hu_mcts_plan(&alloc, goal, 4, ctx, 3, &config, &result), HU_OK);
    HU_ASSERT_TRUE(result.total_iterations <= config.max_iterations);
    hu_mcts_result_free_path(&alloc, &result);
}

static void mcts_plan_respects_max_depth(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t config = hu_mcts_config_default();
    config.max_iterations = 50;
    config.max_depth = 3;
    hu_mcts_result_t result;
    memset(&result, 0, sizeof(result));

    const char *goal = "goal";
    const char *ctx = "ctx";

    HU_ASSERT_EQ(hu_mcts_plan(&alloc, goal, 4, ctx, 3, &config, &result), HU_OK);
    HU_ASSERT_TRUE(result.max_depth_reached <= config.max_depth);
    hu_mcts_result_free_path(&alloc, &result);
}

static void mcts_plan_explores_multiple_nodes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t config = hu_mcts_config_default();
    config.max_iterations = 30;
    hu_mcts_result_t result;
    memset(&result, 0, sizeof(result));

    const char *goal = "explore";
    const char *ctx = "context";

    HU_ASSERT_EQ(hu_mcts_plan(&alloc, goal, 7, ctx, 7, &config, &result), HU_OK);
    HU_ASSERT_TRUE(result.total_nodes > 1);
    hu_mcts_result_free_path(&alloc, &result);
}

static void mcts_backpropagation_updates_values(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcts_config_t config = hu_mcts_config_default();
    config.max_iterations = 15;
    hu_mcts_result_t result;
    memset(&result, 0, sizeof(result));

    const char *goal = "backprop";
    const char *ctx = "ctx";

    HU_ASSERT_EQ(hu_mcts_plan(&alloc, goal, 8, ctx, 3, &config, &result), HU_OK);
    HU_ASSERT_TRUE(result.total_iterations > 0);
    HU_ASSERT_TRUE(result.total_nodes > 0);
    hu_mcts_result_free_path(&alloc, &result);
}

void run_mcts_planner_tests(void) {
    HU_TEST_SUITE("mcts");
    HU_RUN_TEST(mcts_config_default_values);
    HU_RUN_TEST(mcts_plan_null_args_returns_error);
    HU_RUN_TEST(mcts_plan_finds_best_action);
    HU_RUN_TEST(mcts_plan_respects_max_iterations);
    HU_RUN_TEST(mcts_plan_respects_max_depth);
    HU_RUN_TEST(mcts_plan_explores_multiple_nodes);
    HU_RUN_TEST(mcts_backpropagation_updates_values);
}
