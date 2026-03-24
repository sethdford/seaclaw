#include "human/agent/mar.h"
#include "test_framework.h"
#include <string.h>

static void test_mar_role_names(void) {
    HU_ASSERT_STR_EQ(hu_mar_role_name(HU_MAR_ACTOR), "actor");
    HU_ASSERT_STR_EQ(hu_mar_role_name(HU_MAR_DIAGNOSER), "diagnoser");
    HU_ASSERT_STR_EQ(hu_mar_role_name(HU_MAR_CRITIC), "critic");
    HU_ASSERT_STR_EQ(hu_mar_role_name(HU_MAR_JUDGE), "judge");
}

static void test_mar_execute_basic(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_mar_config_t config = HU_MAR_CONFIG_DEFAULT;
    config.enabled = true;
    config.max_rounds = 1;

    hu_mar_result_t result;
    const char *task = "Solve the problem";
    hu_error_t err = hu_mar_execute(&alloc, &provider, &config, task, strlen(task), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.rounds_completed, 1);
    HU_ASSERT_EQ((int)result.phase_count, 4);
    HU_ASSERT_TRUE(result.consensus_reached);
    HU_ASSERT_NOT_NULL(result.final_output);

    HU_ASSERT_EQ((int)result.phases[0].role, (int)HU_MAR_ACTOR);
    HU_ASSERT_EQ((int)result.phases[1].role, (int)HU_MAR_DIAGNOSER);
    HU_ASSERT_EQ((int)result.phases[2].role, (int)HU_MAR_CRITIC);
    HU_ASSERT_EQ((int)result.phases[3].role, (int)HU_MAR_JUDGE);

    for (size_t i = 0; i < result.phase_count; i++)
        HU_ASSERT_TRUE(result.phases[i].success);

    hu_mar_result_free(&alloc, &result);
}

static void test_mar_execute_multi_round(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_mar_config_t config = HU_MAR_CONFIG_DEFAULT;
    config.enabled = true;
    config.max_rounds = 3;

    hu_mar_result_t result;
    const char *task = "Complex analysis";
    hu_error_t err = hu_mar_execute(&alloc, &provider, &config, task, strlen(task), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.rounds_completed, 3);
    HU_ASSERT_EQ((int)result.phase_count, 12);
    hu_mar_result_free(&alloc, &result);
}

static void test_mar_execute_disabled(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_mar_config_t config = HU_MAR_CONFIG_DEFAULT;

    hu_mar_result_t result;
    hu_error_t err = hu_mar_execute(&alloc, &provider, &config, "test", 4, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.phase_count, 0);
    HU_ASSERT_EQ((int)result.rounds_completed, 0);
}

static void test_mar_null_args(void) {
    hu_mar_result_t result;
    HU_ASSERT_EQ(hu_mar_execute(NULL, NULL, NULL, NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void test_mar_result_free_null(void) {
    hu_mar_result_free(NULL, NULL);
    hu_mar_phase_result_free(NULL, NULL);
}

static void test_mar_output_contains_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_mar_config_t config = HU_MAR_CONFIG_DEFAULT;
    config.enabled = true;
    config.max_rounds = 1;

    hu_mar_result_t result;
    const char *task = "unique_marker_xyz";
    hu_mar_execute(&alloc, &provider, &config, task, strlen(task), &result);
    HU_ASSERT_NOT_NULL(result.phases[0].output);
    HU_ASSERT_STR_CONTAINS(result.phases[0].output, "unique_marker_xyz");
    hu_mar_result_free(&alloc, &result);
}

void run_mar_tests(void) {
    HU_TEST_SUITE("MAR Orchestration");
    HU_RUN_TEST(test_mar_role_names);
    HU_RUN_TEST(test_mar_execute_basic);
    HU_RUN_TEST(test_mar_execute_multi_round);
    HU_RUN_TEST(test_mar_execute_disabled);
    HU_RUN_TEST(test_mar_null_args);
    HU_RUN_TEST(test_mar_result_free_null);
    HU_RUN_TEST(test_mar_output_contains_task);
}
