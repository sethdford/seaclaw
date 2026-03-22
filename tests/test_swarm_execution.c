#include "human/agent/swarm.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void swarm_config_default_values(void) {
    hu_swarm_config_t c = hu_swarm_config_default();
    HU_ASSERT_EQ(c.max_parallel, 4);
    HU_ASSERT_EQ(c.timeout_ms, 30000);
    HU_ASSERT_EQ(c.retry_on_failure, 1);
}

static void swarm_execute_all_tasks(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_config_t config = hu_swarm_config_default();
    hu_swarm_task_t tasks[4];
    memset(tasks, 0, sizeof(tasks));
    strncpy(tasks[0].description, "task1", sizeof(tasks[0].description) - 1);
    tasks[0].description_len = 5;
    strncpy(tasks[1].description, "task2", sizeof(tasks[1].description) - 1);
    tasks[1].description_len = 5;
    strncpy(tasks[2].description, "task3", sizeof(tasks[2].description) - 1);
    tasks[2].description_len = 5;
    strncpy(tasks[3].description, "task4", sizeof(tasks[3].description) - 1);
    tasks[3].description_len = 5;

    hu_swarm_result_t result = {0};
    hu_error_t err = hu_swarm_execute(&alloc, &config, tasks, 4, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.task_count, 4u);
    HU_ASSERT_EQ(result.completed, 4u);
    HU_ASSERT_EQ(result.failed, 0u);
    HU_ASSERT_NOT_NULL(result.tasks);
    for (size_t i = 0; i < 4; i++) {
        HU_ASSERT_TRUE(result.tasks[i].completed);
        HU_ASSERT_FALSE(result.tasks[i].failed);
    }
    hu_swarm_result_free(&alloc, &result);
}

static void swarm_handles_empty_task_list(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_config_t config = hu_swarm_config_default();
    hu_swarm_result_t result = {0};
    hu_error_t err = hu_swarm_execute(&alloc, &config, NULL, 0, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.task_count, 0u);
    HU_ASSERT_EQ(result.completed, 0u);
    HU_ASSERT_NULL(result.tasks);
}

static void swarm_tracks_results(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_task_t tasks[2];
    memset(tasks, 0, sizeof(tasks));
    strncpy(tasks[0].description, "a", sizeof(tasks[0].description) - 1);
    tasks[0].description_len = 1;
    strncpy(tasks[1].description, "b", sizeof(tasks[1].description) - 1);
    tasks[1].description_len = 1;

    hu_swarm_result_t result = {0};
    hu_swarm_execute(&alloc, NULL, tasks, 2, &result);
    HU_ASSERT_EQ(result.completed, 2u);
    HU_ASSERT_EQ(result.task_count, 2u);
    hu_swarm_result_free(&alloc, &result);
}

static void swarm_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_task_t tasks[1];
    memset(tasks, 0, sizeof(tasks));
    hu_swarm_result_t result = {0};

    HU_ASSERT_EQ(hu_swarm_execute(NULL, NULL, tasks, 1, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_swarm_execute(&alloc, NULL, tasks, 1, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_swarm_execute(&alloc, NULL, NULL, 1, &result), HU_ERR_INVALID_ARGUMENT);
}

static void swarm_handles_agent_failure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_config_t config = hu_swarm_config_default();
    config.retry_on_failure = 1;
    hu_swarm_task_t tasks[3];
    memset(tasks, 0, sizeof(tasks));
    strncpy(tasks[0].description, "normal_task", sizeof(tasks[0].description) - 1);
    tasks[0].description_len = 11;
    strncpy(tasks[1].description, "will_fail_task", sizeof(tasks[1].description) - 1);
    tasks[1].description_len = 14;
    strncpy(tasks[2].description, "another_task", sizeof(tasks[2].description) - 1);
    tasks[2].description_len = 12;

    hu_swarm_result_t result = {0};
    hu_error_t err = hu_swarm_execute(&alloc, &config, tasks, 3, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.task_count, 3u);
    HU_ASSERT_TRUE(result.failed >= 1);
    HU_ASSERT_TRUE(result.tasks[1].failed);
    hu_swarm_result_free(&alloc, &result);
}

static void swarm_aggregate_concatenate(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_task_t tasks[2];
    memset(tasks, 0, sizeof(tasks));
    strncpy(tasks[0].description, "task_a", sizeof(tasks[0].description) - 1);
    tasks[0].description_len = 6;
    strncpy(tasks[1].description, "task_b", sizeof(tasks[1].description) - 1);
    tasks[1].description_len = 6;

    hu_swarm_result_t result = {0};
    hu_swarm_execute(&alloc, NULL, tasks, 2, &result);

    char agg[4096];
    size_t agg_len = 0;
    HU_ASSERT_EQ(hu_swarm_aggregate(&result, HU_SWARM_AGG_CONCATENATE,
                                     agg, sizeof(agg), &agg_len), HU_OK);
    HU_ASSERT_TRUE(agg_len > 0);
    hu_swarm_result_free(&alloc, &result);
}

static void swarm_aggregate_first_success(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_swarm_task_t tasks[2];
    memset(tasks, 0, sizeof(tasks));
    strncpy(tasks[0].description, "first", sizeof(tasks[0].description) - 1);
    tasks[0].description_len = 5;
    strncpy(tasks[1].description, "second", sizeof(tasks[1].description) - 1);
    tasks[1].description_len = 6;

    hu_swarm_result_t result = {0};
    hu_swarm_execute(&alloc, NULL, tasks, 2, &result);

    char agg[4096];
    size_t agg_len = 0;
    HU_ASSERT_EQ(hu_swarm_aggregate(&result, HU_SWARM_AGG_FIRST_SUCCESS,
                                     agg, sizeof(agg), &agg_len), HU_OK);
    HU_ASSERT_TRUE(agg_len > 0);
    HU_ASSERT_STR_EQ(agg, "mock result");
    hu_swarm_result_free(&alloc, &result);
}

static void swarm_aggregate_null_args(void) {
    char buf[64];
    size_t len = 0;
    HU_ASSERT_EQ(hu_swarm_aggregate(NULL, HU_SWARM_AGG_CONCATENATE, buf, 64, &len),
                 HU_ERR_INVALID_ARGUMENT);
}

static void swarm_aggregate_vote_clusters_similar_phrasings(void) {
    hu_swarm_task_t tasks[3];
    memset(tasks, 0, sizeof(tasks));

    const char *r0 = "The answer is 42";
    const char *r1 = "42 is the answer";
    const char *r2 = "completely different";

    strncpy(tasks[0].result, r0, sizeof(tasks[0].result) - 1);
    tasks[0].result_len = strlen(r0);
    tasks[0].completed = true;

    strncpy(tasks[1].result, r1, sizeof(tasks[1].result) - 1);
    tasks[1].result_len = strlen(r1);
    tasks[1].completed = true;

    strncpy(tasks[2].result, r2, sizeof(tasks[2].result) - 1);
    tasks[2].result_len = strlen(r2);
    tasks[2].completed = true;

    hu_swarm_result_t result = {.tasks = tasks, .task_count = 3};
    char agg[256];
    size_t agg_len = 0;
    HU_ASSERT_EQ(hu_swarm_aggregate(&result, HU_SWARM_AGG_VOTE, agg, sizeof(agg), &agg_len), HU_OK);
    HU_ASSERT_TRUE(agg_len > 0);
    HU_ASSERT_TRUE(strstr(agg, "42") != NULL);
    HU_ASSERT_TRUE(strstr(agg, "different") == NULL);
}

void run_swarm_execution_tests(void) {
    HU_TEST_SUITE("swarm_execution");
    HU_RUN_TEST(swarm_config_default_values);
    HU_RUN_TEST(swarm_execute_all_tasks);
    HU_RUN_TEST(swarm_handles_empty_task_list);
    HU_RUN_TEST(swarm_tracks_results);
    HU_RUN_TEST(swarm_null_args_returns_error);
    HU_RUN_TEST(swarm_handles_agent_failure);
    HU_RUN_TEST(swarm_aggregate_concatenate);
    HU_RUN_TEST(swarm_aggregate_first_success);
    HU_RUN_TEST(swarm_aggregate_null_args);
    HU_RUN_TEST(swarm_aggregate_vote_clusters_similar_phrasings);
}
