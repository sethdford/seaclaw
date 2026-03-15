/* Tests for recursive Tree-of-Thought with beam search (AGI-W3). */
#include "human/agent/tree_of_thought.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

static void tot_recursive_explores_multiple_depths(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tot_config_t cfg = hu_tot_config_default();
    cfg.max_depth = 3;
    hu_tot_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = hu_tot_explore(&alloc, NULL, "gpt-4", 4, "Solve X", 7, &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.max_depth_reached > 1);

    hu_tot_result_free(&alloc, &result);
}

static void tot_beam_search_limits_width(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tot_config_t cfg = hu_tot_config_default();
    cfg.beam_width = 2;
    cfg.max_depth = 3;
    hu_tot_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = hu_tot_explore(&alloc, NULL, "gpt-4", 4, "Solve X", 7, &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.branches_explored >= 3u);

    hu_tot_result_free(&alloc, &result);
}

static void tot_max_nodes_respected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tot_config_t cfg = hu_tot_config_default();
    cfg.max_total_nodes = 20;
    cfg.max_depth = 5;
    hu_tot_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = hu_tot_explore(&alloc, NULL, "gpt-4", 4, "Solve X", 7, &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.branches_explored <= 20u);

    hu_tot_result_free(&alloc, &result);
}

static void tot_best_thought_from_deepest_level(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tot_config_t cfg = hu_tot_config_default();
    cfg.max_depth = 3;
    hu_tot_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = hu_tot_explore(&alloc, NULL, "gpt-4", 4, "Solve X", 7, &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(result.best_thought);
    HU_ASSERT_TRUE(result.best_score > 0.5);
    HU_ASSERT_TRUE(strstr(result.best_thought, "subproblems") != NULL);

    hu_tot_result_free(&alloc, &result);
}

static void tot_config_default_values(void) {
    hu_tot_config_t cfg = hu_tot_config_default();
    HU_ASSERT_EQ(cfg.beam_width, 3);
    HU_ASSERT_EQ(cfg.max_total_nodes, 50);
    HU_ASSERT_EQ(cfg.strategy, 0);
}

static void tot_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tot_config_t cfg = hu_tot_config_default();
    hu_tot_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = hu_tot_explore(NULL, NULL, "gpt-4", 4, "Solve X", 7, &cfg, &result);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    err = hu_tot_explore(&alloc, NULL, "gpt-4", 4, "Solve X", 7, &cfg, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

void run_tot_recursive_tests(void) {
    HU_TEST_SUITE("tot_recursive");
    HU_RUN_TEST(tot_recursive_explores_multiple_depths);
    HU_RUN_TEST(tot_beam_search_limits_width);
    HU_RUN_TEST(tot_max_nodes_respected);
    HU_RUN_TEST(tot_best_thought_from_deepest_level);
    HU_RUN_TEST(tot_config_default_values);
    HU_RUN_TEST(tot_null_args_returns_error);
}
