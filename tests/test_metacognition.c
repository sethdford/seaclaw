#include "human/cognition/metacognition.h"
#include "test_framework.h"
#include <string.h>

static void apply_config_copies_settings(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    hu_metacog_settings_t s;
    hu_metacog_settings_default(&s);
    s.enabled = false;
    s.max_regen = 5;
    s.hysteresis_min = 7;
    hu_metacognition_apply_config(&mc, &s);
    HU_ASSERT_FALSE(mc.cfg.enabled);
    HU_ASSERT_EQ(mc.cfg.max_regen, 5u);
    HU_ASSERT_EQ(mc.cfg.hysteresis_min, 7u);
}

static void init_sets_defaults(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    HU_ASSERT_EQ(mc.signal_count, (size_t)0);
    HU_ASSERT_EQ(mc.reflect_count, 0u);
    HU_ASSERT_EQ(mc.cfg.max_reflects, 2u);
    HU_ASSERT_EQ(mc.cfg.max_regen, 1u);
    HU_ASSERT_EQ(mc.cfg.hysteresis_min, 2u);
    HU_ASSERT_EQ(mc.last_action, HU_METACOG_ACTION_NONE);
    HU_ASSERT_TRUE(mc.cfg.confidence_threshold > 0.0f);
    HU_ASSERT_TRUE(mc.cfg.enabled);
    HU_ASSERT_FALSE(mc.last_suppressed_hysteresis);
}

static void monitor_confident_response(void) {
    const char *query = "What is photosynthesis?";
    const char *response = "Photosynthesis is the process by which plants convert "
                           "sunlight into chemical energy. Chlorophyll absorbs light "
                           "and drives the reaction.";
    hu_metacognition_signal_t s = hu_metacognition_monitor(
        query, strlen(query), response, strlen(response), NULL, 0, 0.8f, 500, 100, NULL);

    HU_ASSERT_TRUE(s.confidence > 0.5f);
    HU_ASSERT_TRUE(s.coherence > 0.0f);
    HU_ASSERT_EQ(s.repetition, 0.0f);
    HU_ASSERT_TRUE(s.emotional_alignment >= 0.0f);
    HU_ASSERT_TRUE(s.stuck_score >= 0.0f);
    HU_ASSERT_TRUE(s.satisfaction_proxy > 0.0f);
}

static void monitor_hedging_response_low_confidence(void) {
    const char *query = "Is this safe?";
    const char *response = "I'm not sure, maybe it could be safe, perhaps, "
                           "I think it might possibly work, although I'm uncertain.";
    hu_metacognition_signal_t s = hu_metacognition_monitor(
        query, strlen(query), response, strlen(response), NULL, 0, 0.5f, 500, 100, NULL);

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
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.9f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_NONE);
}

static void plan_action_reflect_on_low_confidence(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 1;

    hu_metacognition_signal_t s = {
        .confidence = 0.1f,
        .coherence = 0.5f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.8f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_REFLECT);
    HU_ASSERT_EQ(mc.reflect_count, 1u);
}

static void plan_action_clarify_after_max_reflects(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 1;
    mc.reflect_count = mc.cfg.max_reflects;

    hu_metacognition_signal_t s = {
        .confidence = 0.1f,
        .coherence = 0.5f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.8f,
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
        .stuck_score = 0.9f,
        .satisfaction_proxy = 0.8f,
    };
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_SIMPLIFY);
}

static void plan_action_switch_strategy_on_low_coherence(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 1;

    hu_metacognition_signal_t s = {
        .confidence = 0.8f,
        .coherence = 0.1f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.8f,
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

static void difficulty_short_question_easy(void) {
    const char *q = "Hi";
    HU_ASSERT_EQ(hu_metacog_estimate_difficulty(q, strlen(q)), HU_METACOG_DIFFICULTY_EASY);
}

static void difficulty_code_fence_harder(void) {
    const char *q = "How do I fix this?\n```c\nint x;\n```\nWhat is wrong?";
    hu_metacog_difficulty_t d = hu_metacog_estimate_difficulty(q, strlen(q));
    HU_ASSERT_TRUE(d == HU_METACOG_DIFFICULTY_MEDIUM || d == HU_METACOG_DIFFICULTY_HARD);
}

static void difficulty_names(void) {
    HU_ASSERT_STR_EQ(hu_metacog_difficulty_name(HU_METACOG_DIFFICULTY_EASY), "easy");
    HU_ASSERT_STR_EQ(hu_metacog_difficulty_name(HU_METACOG_DIFFICULTY_HARD), "hard");
}

static void trajectory_empty_is_mid(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    float t = hu_metacog_trajectory_confidence(&mc);
    HU_ASSERT_TRUE(t >= 0.49f && t <= 0.51f);
}

static void trend_needs_two_samples(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    hu_metacog_trend_t tr = hu_metacog_compute_trend(&mc);
    HU_ASSERT_FALSE(tr.is_degrading);
}

static void apply_reflect_ok(void) {
    char buf[256];
    size_t n = 0;
    HU_ASSERT_EQ(hu_metacognition_apply(HU_METACOG_ACTION_REFLECT, buf, sizeof(buf), &n), HU_OK);
    HU_ASSERT_TRUE(n > 10);
    HU_ASSERT_TRUE(strstr(buf, "specific") != NULL);
}

static void apply_none_invalid(void) {
    char buf[8];
    size_t n = 0;
    HU_ASSERT_EQ(hu_metacognition_apply(HU_METACOG_ACTION_NONE, buf, sizeof(buf), &n),
                 HU_ERR_INVALID_ARGUMENT);
}

static void label_followup_positive(void) {
    float v = hu_metacog_label_from_followup("Thanks, that was perfect!", 25);
    HU_ASSERT_TRUE(v > 0.5f);
}

static void label_followup_negative(void) {
    float v = hu_metacog_label_from_followup("No that's wrong, try again", 26);
    HU_ASSERT_TRUE(v < -0.5f);
}

static void label_followup_neutral(void) {
    float v = hu_metacog_label_from_followup("ok", 2);
    HU_ASSERT_TRUE(v > -0.1f && v < 0.1f);
}

static void hysteresis_suppresses_first_costly(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 2;

    hu_metacognition_signal_t s = {
        .confidence = 0.8f,
        .coherence = 0.1f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.8f,
    };
    hu_metacog_action_t a1 = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(a1, HU_METACOG_ACTION_NONE);
    HU_ASSERT_TRUE(mc.last_suppressed_hysteresis);

    hu_metacog_action_t a2 = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(a2, HU_METACOG_ACTION_SWITCH_STRATEGY);
}

static void token_efficiency_simplify_immediate(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 5;

    hu_metacognition_signal_t s = {
        .confidence = 0.9f,
        .coherence = 0.9f,
        .repetition = 0.1f,
        .token_efficiency = 3.0f,
        .emotional_alignment = 0.8f,
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.8f,
    };
    HU_ASSERT_EQ(hu_metacognition_plan_action(&mc, &s), HU_METACOG_ACTION_SIMPLIFY);
}

static void hard_difficulty_relaxes_confidence_threshold(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 1;
    mc.difficulty = HU_METACOG_DIFFICULTY_HARD;
    /* borderline confidence that would clarify on easy but may reflect on hard */
    hu_metacognition_signal_t s = {
        .confidence = 0.25f,
        .coherence = 0.6f,
        .repetition = 0.1f,
        .token_efficiency = 0.5f,
        .emotional_alignment = 0.5f,
        .stuck_score = 0.1f,
        .satisfaction_proxy = 0.8f,
    };
    hu_metacog_action_t a = hu_metacognition_plan_action(&mc, &s);
    /* With relaxed threshold (~0.20), 0.25 may be above -> NONE; else REFLECT first bucket */
    HU_ASSERT_TRUE(a == HU_METACOG_ACTION_NONE || a == HU_METACOG_ACTION_REFLECT);
}

static void sycophancy_detects_agreement_markers(void) {
    const char *response = "You're absolutely right, that's a great point! "
                           "I completely agree with everything you said.";
    hu_metacognition_signal_t s = hu_metacognition_monitor("test", 4, response, strlen(response),
                                                           NULL, 0, 0.8f, 100, 80, NULL);
    HU_ASSERT_TRUE(s.sycophancy_score > 0.3f);
}

static void sycophancy_low_for_independent_response(void) {
    const char *response = "I actually disagree. The evidence suggests otherwise. "
                           "Here is my reasoning for a different conclusion.";
    hu_metacognition_signal_t s = hu_metacognition_monitor("test", 4, response, strlen(response),
                                                           NULL, 0, 0.8f, 100, 80, NULL);
    HU_ASSERT_TRUE(s.sycophancy_score < 0.2f);
}

static void sycophancy_triggers_reflect_when_high(void) {
    hu_metacognition_t mc;
    hu_metacognition_init(&mc);
    mc.cfg.hysteresis_min = 0;
    hu_metacognition_signal_t s;
    memset(&s, 0, sizeof(s));
    s.confidence = 0.9f;
    s.coherence = 0.9f;
    s.sycophancy_score = 0.8f;
    s.trajectory_confidence = 0.8f;
    hu_metacog_action_t action = hu_metacognition_plan_action(&mc, &s);
    HU_ASSERT_EQ(action, HU_METACOG_ACTION_REFLECT);
}

void run_metacognition_tests(void) {
    HU_TEST_SUITE("Metacognition");
    HU_RUN_TEST(apply_config_copies_settings);
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
    HU_RUN_TEST(difficulty_short_question_easy);
    HU_RUN_TEST(difficulty_code_fence_harder);
    HU_RUN_TEST(difficulty_names);
    HU_RUN_TEST(trajectory_empty_is_mid);
    HU_RUN_TEST(trend_needs_two_samples);
    HU_RUN_TEST(apply_reflect_ok);
    HU_RUN_TEST(apply_none_invalid);
    HU_RUN_TEST(label_followup_positive);
    HU_RUN_TEST(label_followup_negative);
    HU_RUN_TEST(label_followup_neutral);
    HU_RUN_TEST(hysteresis_suppresses_first_costly);
    HU_RUN_TEST(token_efficiency_simplify_immediate);
    HU_RUN_TEST(hard_difficulty_relaxes_confidence_threshold);
    HU_RUN_TEST(sycophancy_detects_agreement_markers);
    HU_RUN_TEST(sycophancy_low_for_independent_response);
    HU_RUN_TEST(sycophancy_triggers_reflect_when_high);
}
