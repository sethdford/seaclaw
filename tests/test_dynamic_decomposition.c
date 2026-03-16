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

void run_dynamic_decomposition_tests(void) {
    HU_TEST_SUITE("dynamic_decomposition");
    HU_RUN_TEST(decomposition_parallel_strategy);
    HU_RUN_TEST(decomposition_sequential_strategy);
    HU_RUN_TEST(decomposition_null_args_returns_error);
}
