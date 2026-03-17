#include "human/agent/orchestrator_llm.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void decomposition_parallel_strategy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_decomposition_result_t result = {0};
    hu_error_t err = hu_decompose_task(&alloc, NULL, NULL, 0, "do research", 10,
                                       HU_DECOMP_PARALLEL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.task_count, 3u);
    HU_ASSERT_EQ(result.strategy, HU_DECOMP_PARALLEL);
    /* Mock: 2 parallel + 1 dependent */
    HU_ASSERT_EQ(result.tasks[0].depends_on, 0u);
    HU_ASSERT_EQ(result.tasks[1].depends_on, 0u);
    HU_ASSERT_TRUE(result.tasks[2].depends_on != 0);
}

static void decomposition_sequential_strategy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_decomposition_result_t result = {0};
    hu_error_t err = hu_decompose_task(&alloc, NULL, NULL, 0, "build pipeline", 14,
                                       HU_DECOMP_SEQUENTIAL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.task_count, 3u);
    HU_ASSERT_EQ(result.strategy, HU_DECOMP_SEQUENTIAL);
    HU_ASSERT_TRUE(result.tasks[0].description_len > 0);
    HU_ASSERT_TRUE(result.tasks[2].depends_on != 0);
}

static void decomposition_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_decomposition_result_t result = {0};

    HU_ASSERT_EQ(hu_decompose_task(NULL, NULL, NULL, 0, "x", 1, HU_DECOMP_PARALLEL, &result),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_decompose_task(&alloc, NULL, NULL, 0, "x", 1, HU_DECOMP_PARALLEL, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void decomposition_replan_produces_new_tasks(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_decomposition_result_t result = {0};
    hu_error_t err = hu_decompose_with_replan(&alloc, NULL, NULL, 0,
        "build a website", 15,
        "deploy to server", 16,
        "server unavailable", 18,
        HU_DECOMP_SEQUENTIAL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.task_count >= 2);
    HU_ASSERT_TRUE(result.tasks[0].description_len > 0);
}

static void decomposition_replan_null_args(void) {
    hu_decomposition_result_t result = {0};
    HU_ASSERT_EQ(hu_decompose_with_replan(NULL, NULL, NULL, 0, "x", 1,
                                           "y", 1, "z", 1, HU_DECOMP_PARALLEL, &result),
                 HU_ERR_INVALID_ARGUMENT);
}

static void decomposition_coverage_check_passes_on_match(void) {
    hu_decomposition_result_t result = {0};
    result.task_count = 2;
    strncpy(result.tasks[0].description, "research machine learning algorithms",
            sizeof(result.tasks[0].description) - 1);
    result.tasks[0].description_len = 36;
    strncpy(result.tasks[1].description, "implement neural network training",
            sizeof(result.tasks[1].description) - 1);
    result.tasks[1].description_len = 33;

    bool covered = hu_decomposition_check_coverage(
        "research machine learning and implement training", 49, &result);
    HU_ASSERT_TRUE(covered);
}

static void decomposition_coverage_check_fails_on_mismatch(void) {
    hu_decomposition_result_t result = {0};
    result.task_count = 1;
    strncpy(result.tasks[0].description, "write documentation",
            sizeof(result.tasks[0].description) - 1);
    result.tasks[0].description_len = 19;

    bool covered = hu_decomposition_check_coverage(
        "deploy kubernetes cluster to production servers", 47, &result);
    HU_ASSERT_TRUE(!covered);
}

static void decomposition_coverage_empty_returns_false(void) {
    hu_decomposition_result_t result = {0};
    HU_ASSERT_TRUE(!hu_decomposition_check_coverage("goal", 4, &result));
    HU_ASSERT_TRUE(!hu_decomposition_check_coverage(NULL, 0, &result));
}

void run_dynamic_decomposition_tests(void) {
    HU_TEST_SUITE("dynamic_decomposition");
    HU_RUN_TEST(decomposition_parallel_strategy);
    HU_RUN_TEST(decomposition_sequential_strategy);
    HU_RUN_TEST(decomposition_null_args_returns_error);
    HU_RUN_TEST(decomposition_replan_produces_new_tasks);
    HU_RUN_TEST(decomposition_replan_null_args);
    HU_RUN_TEST(decomposition_coverage_check_passes_on_match);
    HU_RUN_TEST(decomposition_coverage_check_fails_on_mismatch);
    HU_RUN_TEST(decomposition_coverage_empty_returns_false);
}
