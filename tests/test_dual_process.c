#include "human/cognition/dual_process.h"
#include "test_framework.h"
#include <string.h>

static void greeting_dispatches_fast(void) {
    hu_cognition_dispatch_input_t input = {
        .message = "hey",
        .message_len = 3,
        .emotional = NULL,
        .tools_count = 5,
        .recent_tool_calls = 0,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_FAST);
}

static void thanks_dispatches_fast(void) {
    hu_cognition_dispatch_input_t input = {
        .message = "thanks!",
        .message_len = 7,
        .emotional = NULL,
        .tools_count = 5,
        .recent_tool_calls = 0,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_FAST);
}

static void short_statement_dispatches_fast(void) {
    hu_cognition_dispatch_input_t input = {
        .message = "ok cool",
        .message_len = 7,
        .emotional = NULL,
        .tools_count = 5,
        .recent_tool_calls = 0,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_FAST);
}

static void question_dispatches_slow(void) {
    const char *msg = "How does the agent turn pipeline work in agent_turn.c?";
    hu_cognition_dispatch_input_t input = {
        .message = msg,
        .message_len = strlen(msg),
        .emotional = NULL,
        .tools_count = 5,
        .recent_tool_calls = 0,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_SLOW);
}

static void complex_task_dispatches_slow(void) {
    const char *msg = "Please implement a new provider following the vtable pattern.";
    hu_cognition_dispatch_input_t input = {
        .message = msg,
        .message_len = strlen(msg),
        .emotional = NULL,
        .tools_count = 5,
        .recent_tool_calls = 0,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_SLOW);
}

static void high_emotion_dispatches_emotional(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    ec.state.intensity = 0.8f;
    ec.state.concerning = true;

    hu_cognition_dispatch_input_t input = {
        .message = "I'm really struggling with this",
        .message_len = 31,
        .emotional = &ec,
        .tools_count = 5,
        .recent_tool_calls = 0,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_EMOTIONAL);
}

static void recent_tools_biases_slow(void) {
    hu_cognition_dispatch_input_t input = {
        .message = "continue",
        .message_len = 8,
        .emotional = NULL,
        .tools_count = 5,
        .recent_tool_calls = 3,
        .agent_max_tool_iterations = 10,
    };
    hu_cognition_mode_t mode = hu_cognition_dispatch(&input);
    HU_ASSERT_EQ(mode, HU_COGNITION_SLOW);
}

static void null_input_returns_fast(void) {
    hu_cognition_mode_t mode = hu_cognition_dispatch(NULL);
    HU_ASSERT_EQ(mode, HU_COGNITION_FAST);
}

static void budget_fast_has_low_limits(void) {
    hu_cognition_budget_t b = hu_cognition_get_budget(HU_COGNITION_FAST, 10);
    HU_ASSERT_EQ(b.max_memory_entries, (size_t)3);
    HU_ASSERT_EQ(b.max_tool_iterations, 2u);
    HU_ASSERT_TRUE(!b.enable_planning);
    HU_ASSERT_TRUE(!b.enable_tree_of_thought);
    HU_ASSERT_TRUE(!b.enable_reflection);
}

static void budget_slow_has_full_limits(void) {
    hu_cognition_budget_t b = hu_cognition_get_budget(HU_COGNITION_SLOW, 10);
    HU_ASSERT_EQ(b.max_memory_entries, (size_t)10);
    HU_ASSERT_EQ(b.max_tool_iterations, 10u);
    HU_ASSERT_TRUE(b.enable_planning);
    HU_ASSERT_TRUE(b.enable_tree_of_thought);
    HU_ASSERT_TRUE(b.enable_reflection);
}

static void budget_emotional_prioritizes_empathy(void) {
    hu_cognition_budget_t b = hu_cognition_get_budget(HU_COGNITION_EMOTIONAL, 10);
    HU_ASSERT_TRUE(b.prioritize_empathy);
    HU_ASSERT_TRUE(!b.enable_planning);
    HU_ASSERT_TRUE(b.enable_reflection);
}

static void mode_names_are_valid(void) {
    HU_ASSERT_STR_EQ(hu_cognition_mode_name(HU_COGNITION_FAST), "fast");
    HU_ASSERT_STR_EQ(hu_cognition_mode_name(HU_COGNITION_SLOW), "slow");
    HU_ASSERT_STR_EQ(hu_cognition_mode_name(HU_COGNITION_EMOTIONAL), "emotional");
}

void run_dual_process_tests(void) {
    HU_TEST_SUITE("DualProcess");
    HU_RUN_TEST(greeting_dispatches_fast);
    HU_RUN_TEST(thanks_dispatches_fast);
    HU_RUN_TEST(short_statement_dispatches_fast);
    HU_RUN_TEST(question_dispatches_slow);
    HU_RUN_TEST(complex_task_dispatches_slow);
    HU_RUN_TEST(high_emotion_dispatches_emotional);
    HU_RUN_TEST(recent_tools_biases_slow);
    HU_RUN_TEST(null_input_returns_fast);
    HU_RUN_TEST(budget_fast_has_low_limits);
    HU_RUN_TEST(budget_slow_has_full_limits);
    HU_RUN_TEST(budget_emotional_prioritizes_empathy);
    HU_RUN_TEST(mode_names_are_valid);
}
