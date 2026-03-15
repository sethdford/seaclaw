#include "human/agent/autonomy.h"
#include "test_framework.h"
#include <string.h>

static void test_autonomy_init_defaults(void) {
    hu_autonomy_state_t state;
    hu_error_t err = hu_autonomy_init(&state, 8192);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(state.context_budget, 8192u);
    HU_ASSERT_EQ(state.goal_count, 0u);
}

static void test_autonomy_add_goal(void) {
    hu_autonomy_state_t state;
    hu_autonomy_init(&state, 8192);
    hu_error_t err = hu_autonomy_add_goal(&state, "Complete report", 15, 0.8);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(state.goal_count, 1u);
    HU_ASSERT_STR_EQ(state.goals[0].description, "Complete report");
    HU_ASSERT_EQ(state.goals[0].priority, 0.8);
    HU_ASSERT_FALSE(state.goals[0].completed);
}

static void test_autonomy_get_next_goal(void) {
    hu_autonomy_state_t state;
    hu_autonomy_init(&state, 8192);
    hu_autonomy_add_goal(&state, "Low priority", 12, 0.2);
    hu_autonomy_add_goal(&state, "High priority", 13, 0.9);

    hu_autonomy_goal_t out;
    hu_error_t err = hu_autonomy_get_next_goal(&state, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out.description, "High priority");
    HU_ASSERT_EQ(out.priority, 0.9);
}

static void test_autonomy_mark_complete(void) {
    hu_autonomy_state_t state;
    hu_autonomy_init(&state, 8192);
    hu_autonomy_add_goal(&state, "Task", 4, 0.5);
    hu_error_t err = hu_autonomy_mark_complete(&state, 0);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(state.goals[0].completed);
}

static void test_autonomy_needs_consolidation_over_budget(void) {
    hu_autonomy_state_t state;
    hu_autonomy_init(&state, 100);
    state.context_tokens_used = 85; /* > 80% of 100 */
    bool need = hu_autonomy_needs_consolidation(&state, state.session_start + 1000);
    HU_ASSERT_TRUE(need);
}

static void test_autonomy_consolidate_resets(void) {
    hu_autonomy_state_t state;
    hu_autonomy_init(&state, 8192);
    hu_autonomy_add_goal(&state, "Done task", 9, 0.5);
    hu_autonomy_mark_complete(&state, 0);
    state.context_tokens_used = 1000;

    hu_error_t err = hu_autonomy_consolidate(&state);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(state.context_tokens_used, 0u);
    HU_ASSERT_EQ(state.goal_count, 0u);
}

static void test_autonomy_null_args_returns_error(void) {
    HU_ASSERT_EQ(hu_autonomy_init(NULL, 8192), HU_ERR_INVALID_ARGUMENT);
    hu_autonomy_state_t state;
    hu_autonomy_init(&state, 8192);
    HU_ASSERT_EQ(hu_autonomy_add_goal(NULL, "x", 1, 0.5), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_autonomy_add_goal(&state, NULL, 0, 0.5), HU_ERR_INVALID_ARGUMENT);
    hu_autonomy_goal_t out;
    HU_ASSERT_EQ(hu_autonomy_get_next_goal(NULL, &out), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_autonomy_get_next_goal(&state, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_autonomy_mark_complete(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_autonomy_consolidate(NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_autonomy_tests(void) {
    HU_TEST_SUITE("Autonomy");
    HU_RUN_TEST(test_autonomy_init_defaults);
    HU_RUN_TEST(test_autonomy_add_goal);
    HU_RUN_TEST(test_autonomy_get_next_goal);
    HU_RUN_TEST(test_autonomy_mark_complete);
    HU_RUN_TEST(test_autonomy_needs_consolidation_over_budget);
    HU_RUN_TEST(test_autonomy_consolidate_resets);
    HU_RUN_TEST(test_autonomy_null_args_returns_error);
}
