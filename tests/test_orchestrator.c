#include "test_framework.h"
#include "human/agent/orchestrator.h"
#include "human/agent/orchestrator_llm.h"
#include "human/core/allocator.h"
#include <string.h>

static void test_orchestrator_decompose_goal_returns_research_synthesize(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_decomposition_t result;
    memset(&result, 0, sizeof(result));

    HU_ASSERT_EQ(hu_orchestrator_decompose_goal(&alloc, NULL, "gpt-4", 5, "complex goal", 12, NULL, 0,
                                                &result),
                 HU_OK);
    HU_ASSERT_EQ(result.task_count, 2u);
    HU_ASSERT_STR_EQ(result.tasks[0].description, "research");
    HU_ASSERT_STR_EQ(result.tasks[1].description, "synthesize");
    hu_decomposition_free(&alloc, &result);
}

static void test_orchestrator_merge_consensus_picks_longest_when_similar(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    const char *tasks[] = {"a", "b", "c"};
    size_t lens[] = {1, 1, 1};
    hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 3);
    hu_orchestrator_assign_task(&orch, 1, "x", 1);
    hu_orchestrator_assign_task(&orch, 2, "y", 1);
    hu_orchestrator_assign_task(&orch, 3, "z", 1);

    hu_orchestrator_complete_task(&orch, 1, "the quick brown fox", 19);
    hu_orchestrator_complete_task(&orch, 2, "quick brown fox jumps", 21);
    hu_orchestrator_complete_task(&orch, 3, "the quick brown fox jumps over", 30);

    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_TRUE(merged_len > 0);
    HU_ASSERT_STR_EQ(merged, "the quick brown fox jumps over");
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_divergent_keeps_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    const char *tasks[] = {"a", "b"};
    size_t lens[] = {1, 1};
    hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 2);
    hu_orchestrator_assign_task(&orch, 1, "x", 1);
    hu_orchestrator_assign_task(&orch, 2, "y", 1);
    hu_orchestrator_complete_task(&orch, 1, "apple banana cherry", 19);
    hu_orchestrator_complete_task(&orch, 2, "xray zebra quantum", 18);

    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_TRUE(merged_len > 40);
    HU_ASSERT_TRUE(strstr(merged, "Multiple perspectives") != NULL);
    HU_ASSERT_TRUE(strstr(merged, "apple") != NULL);
    HU_ASSERT_TRUE(strstr(merged, "zebra") != NULL);
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_single_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    hu_orchestrator_create(&alloc, &orch);
    const char *tasks[] = {"only"};
    size_t lens[] = {4};
    hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 1);
    hu_orchestrator_assign_task(&orch, 1, "a", 1);
    hu_orchestrator_complete_task(&orch, 1, "solo result", 11);

    char *merged = NULL;
    size_t merged_len = 0;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT_STR_EQ(merged, "solo result");
    alloc.free(alloc.ctx, merged, merged_len + 1);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_fresh_no_tasks_returns_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    char *merged = NULL;
    size_t merged_len = 1;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NULL(merged);
    HU_ASSERT_EQ(merged_len, 0u);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_consensus_all_failed_returns_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    const char *tasks[] = {"a", "b"};
    size_t lens[] = {1, 1};
    HU_ASSERT_EQ(hu_orchestrator_propose_split(&orch, "g", 1, tasks, lens, 2), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_assign_task(&orch, 1, "x", 1), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_assign_task(&orch, 2, "y", 1), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_fail_task(&orch, 1, "e1", 2), HU_OK);
    HU_ASSERT_EQ(hu_orchestrator_fail_task(&orch, 2, "e2", 2), HU_OK);

    char *merged = NULL;
    size_t merged_len = 1;
    HU_ASSERT_EQ(hu_orchestrator_merge_results_consensus(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NULL(merged);
    HU_ASSERT_EQ(merged_len, 0u);
    hu_orchestrator_deinit(&orch);
}

static void test_orchestrator_merge_results_empty_completed_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_orchestrator_t orch;
    HU_ASSERT_EQ(hu_orchestrator_create(&alloc, &orch), HU_OK);

    char *merged = NULL;
    size_t merged_len = 1;
    HU_ASSERT_EQ(hu_orchestrator_merge_results(&orch, &alloc, &merged, &merged_len), HU_OK);
    HU_ASSERT_NULL(merged);
    HU_ASSERT_EQ(merged_len, 0u);
    hu_orchestrator_deinit(&orch);
}

void run_orchestrator_tests(void) {
    HU_TEST_SUITE("Orchestrator");

    HU_RUN_TEST(test_orchestrator_decompose_goal_returns_research_synthesize);
    HU_RUN_TEST(test_orchestrator_merge_consensus_picks_longest_when_similar);
    HU_RUN_TEST(test_orchestrator_merge_consensus_divergent_keeps_all);
    HU_RUN_TEST(test_orchestrator_merge_consensus_single_task);
    HU_RUN_TEST(test_orchestrator_merge_consensus_fresh_no_tasks_returns_empty);
    HU_RUN_TEST(test_orchestrator_merge_consensus_all_failed_returns_empty);
    HU_RUN_TEST(test_orchestrator_merge_results_empty_completed_returns_ok);
}
