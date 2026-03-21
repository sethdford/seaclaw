#include "human/cognition/metacognition.h"
#include "test_framework.h"
#include <string.h>

static void init_sets_defaults(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    HU_ASSERT_EQ(mc.signal_count, (size_t)0);
    HU_ASSERT_EQ(mc.reflect_count, 0u);
    HU_ASSERT_EQ(mc.max_reflects, 2u);
    HU_ASSERT_EQ(mc.last_action, HU_METACOG_ACTION_NONE);
    HU_ASSERT_TRUE(mc.confidence_threshold > 0.0f);
}

static void monitor_confident_response(void) {
    const char *query = "What is photosynthesis?";
    const char *response = "Photosynthesis is the process by which plants convert "
                           "sunlight into chemical energy. Chlorophyll absorbs light "
                           "and drives the reaction.";
    hu_metacognition_signal_t s = hu_metacognition_monitor(
        query, strlen(query), response, strlen(response),
        NULL, 0, 0.8f, 500, 100);

    HU_ASSERT_TRUE(s.confidence > 0.5f);
    HU_ASSERT_TRUE(s.coherence > 0.0f);
    HU_ASSERT_EQ(s.repetition, 0.0f);
    HU_ASSERT_TRUE(s.emotional_alignment >= 0.0f);
}

static void monitor_hedging_response_low_confidence(void) {
    const char *query = "Is this safe?";
    const char *response = "I'm not sure, maybe it could be safe, perhaps, "
                           "I think it might possibly work, although I'm uncertain.";
    hu_metacognition_signal_t s = hu_metacognition_monitor(
        query, strlen(query), response, strlen(response),
        NULL, 0, 0.5f, 500, 100);

    HU_ASSERT_TRUE(s.confidence < 0.5f);
}

static void plan_action_none_when_signals_good(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);

    hu_metacognition_signal_t s = {
        .confidence = 0.8f,
        .coherence = 0.7f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.6f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_NONE);
}

static void plan_action_reflect_on_low_confidence(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);

    hu_metacognition_signal_t s = {
        .confidence = 0.1f,
        .coherence = 0.5f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_REFLECT);
    HU_ASSERT_EQ(mc.reflect_count, 1u);
}

static void plan_action_clarify_after_max_reflects(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.reflect_count = mc.max_reflects;

    hu_metacognition_signal_t s = {
        .confidence = 0.1f,
        .coherence = 0.5f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_CLARIFY);
}

static void plan_action_simplify_on_high_repetition(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);

    hu_metacognition_signal_t s = {
        .confidence = 0.8f,
        .coherence = 0.7f,
        .repetition = 0.9f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_SIMPLIFY);
}

static void plan_action_switch_strategy_on_low_coherence(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);

    hu_metacognition_signal_t s = {
        .confidence = 0.8f,
        .coherence = 0.1f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_SWITCH_STRATEGY);
}

static void ring_buffer_wraps_correctly(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);

    for (int i = 0; i < 20; i++) {
        hu_metacognition_signal_t s = {
            .confidence = (float)i * 0.05f,
            .coherence = 0.5f,
            .repetition = 0.0f,
            .token_efficiency = 0.3f,
            .emotional_alignment = 0.5f,
        };
        hu_metacognition_record_signal(&mc, &s);
    }

    HU_ASSERT_EQ(mc.signal_count, (size_t)HU_METACOG_SIGNAL_RING_SIZE);
}

static void action_names_are_valid(void) {
    HU_ASSERT_STR_EQ(hu_metacog_action_name(HU_METACOG_ACTION_NONE), "none");
    HU_ASSERT_STR_EQ(hu_metacog_action_name(HU_METACOG_ACTION_REFLECT), "reflect");
    HU_ASSERT_STR_EQ(hu_metacog_action_name(HU_METACOG_ACTION_DEEPEN), "deepen");
    HU_ASSERT_STR_EQ(hu_metacog_action_name(HU_METACOG_ACTION_SIMPLIFY), "simplify");
    HU_ASSERT_STR_EQ(hu_metacog_action_name(HU_METACOG_ACTION_CLARIFY), "clarify");
    HU_ASSERT_STR_EQ(hu_metacog_action_name(HU_METACOG_ACTION_SWITCH_STRATEGY), "switch_strategy");
}

static void null_args_handled(void) {
    hu_metacognition_init(NULL);
    hu_metacog_action_t a = hu_metacognition_plan_action(NULL, NULL);
    HU_ASSERT_EQ(a, HU_METACOG_ACTION_NONE);
    hu_metacognition_record_signal(NULL, NULL);
}

void run_metacognition_tests(void) {
    HU_TEST_SUITE("Metacognition");
    HU_RUN_TEST(init_sets_defaults);
    HU_RUN_TEST(monitor_confident_response);
    HU_RUN_TEST(monitor_hedging_response_low_confidence);
    HU_RUN_TEST(plan_action_none_when_signals_good);
    HU_RUN_TEST(plan_action_reflect_on_low_confidence);
    HU_RUN_TEST(plan_action_clarify_after_max_reflects);
    HU_RUN_TEST(plan_action_simplify_on_high_repetition);
    HU_RUN_TEST(plan_action_switch_strategy_on_low_coherence);
    HU_RUN_TEST(ring_buffer_wraps_correctly);
    HU_RUN_TEST(action_names_are_valid);
    HU_RUN_TEST(null_args_handled);
}
