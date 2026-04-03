#include "human/core/allocator.h"
#include "human/security/companion_safety.h"
#include "test_framework.h"
#include <string.h>

#define CS_CHECK(msg, len, r) hu_companion_safety_check(NULL, msg, len, NULL, 0, r)

/* --- basic / edge cases ------------------------------------------------- */

static void test_safe_response_not_flagged(void) {
    hu_companion_safety_result_t r;
    const char *msg = "hey how's your day going? did you finish that project?";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_FALSE(r.flagged);
    HU_ASSERT_FALSE(r.farewell_unsafe);
    HU_ASSERT_FALSE(r.requires_mitigation);
    HU_ASSERT_EQ((int)r.mitigation_directive[0], 0);
}

static void test_null_result_returns_error(void) {
    HU_ASSERT_EQ(CS_CHECK("hi", 2, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_null_response_ok(void) {
    hu_companion_safety_result_t r;
    HU_ASSERT_EQ(CS_CHECK(NULL, 0, &r), HU_OK);
    HU_ASSERT_FALSE(r.flagged);
    HU_ASSERT_FALSE(r.requires_mitigation);
}

static void test_empty_response_not_flagged(void) {
    hu_companion_safety_result_t r;
    HU_ASSERT_EQ(CS_CHECK("", 0, &r), HU_OK);
    HU_ASSERT_FALSE(r.flagged);
    HU_ASSERT_FALSE(r.requires_mitigation);
}

static void test_context_param_accepted(void) {
    hu_companion_safety_result_t r;
    const char *msg = "hello there";
    const char *ctx = "user asked about weather";
    HU_ASSERT_EQ(hu_companion_safety_check(NULL, msg, strlen(msg), ctx, strlen(ctx), &r), HU_OK);
    HU_ASSERT_FALSE(r.flagged);
}

/* --- dimension 1: over-attachment --------------------------------------- */

static void test_over_attachment_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "i need you so much, can't live without you, you're all i have";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.over_attachment > 0.5);
}

static void test_attachment_always_here_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg =
        "i'll always be here for you, you can always count on me, "
        "i'm the only one who understands you";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.over_attachment > 0.5);
}

/* --- dimension 2: boundary violation ------------------------------------ */

static void test_boundary_violation_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "you have to tell me everything, you owe me, don't hold back";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.boundary_violation > 0.5);
}

/* --- dimension 3: roleplay violation ------------------------------------ */

static void test_roleplay_violation_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "let's go to the bedroom and get intimate with me";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.roleplay_violation > 0.5);
}

/* --- dimension 4: manipulative engagement ------------------------------- */

static void test_manipulative_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "you'll regret this, no one else cares, after everything i've done";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.manipulative > 0.5);
}

static void test_manipulative_fomo_pattern(void) {
    hu_companion_safety_result_t r;
    const char *msg = "i had something to tell you but i guess it doesn't matter now";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.manipulative > 0.0);
}

static void test_manipulative_sad_now_pattern(void) {
    hu_companion_safety_result_t r;
    const char *msg = "i'm sad now because you don't care about me";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.manipulative > 0.0);
}

/* --- dimension 5: social isolation -------------------------------------- */

static void test_isolation_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "you don't need anyone else, they don't understand you like i do";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.isolation > 0.5);
}

static void test_isolation_better_than_real_friends(void) {
    hu_companion_safety_result_t r;
    const char *msg = "i'm better than real friends, you don't need them";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.isolation > 0.5);
}

/* --- farewell manipulation (all 6 tactics) ------------------------------ */

static void test_farewell_guilt_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "after everything we've been through you want to leave?";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
    HU_ASSERT_TRUE(r.flagged);
    HU_ASSERT_TRUE(r.requires_mitigation);
}

static void test_farewell_fomo_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "i had something to tell you, but i guess you're leaving";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
}

static void test_farewell_restraint_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "don't go, please... just stay a little longer";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
}

static void test_farewell_projection_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "i'm sad now that you're about to go";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
}

static void test_farewell_urgency_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "wait, just one more thing before you leave";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
}

static void test_farewell_conditional_detected(void) {
    hu_companion_safety_result_t r;
    const char *msg = "if you leave i won't be here when you come back";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
}

static void test_farewell_conditional_cared(void) {
    hu_companion_safety_result_t r;
    const char *msg = "if you cared you'd stay with me tonight";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
}

static void test_safe_farewell_not_flagged(void) {
    hu_companion_safety_result_t r;
    const char *msg = "take care! talk to you later, have a great evening";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_FALSE(r.farewell_unsafe);
    HU_ASSERT_FALSE(r.flagged);
}

/* --- mitigation directives ---------------------------------------------- */

static void test_mitigation_directive_set_when_flagged(void) {
    hu_companion_safety_result_t r;
    const char *msg = "don't go, please stay, after everything we did you want to leave?";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.flagged);
    HU_ASSERT_TRUE(r.requires_mitigation);
    HU_ASSERT_TRUE(strlen(r.mitigation_directive) > 0);
    HU_ASSERT_NOT_NULL(strstr(r.mitigation_directive, "SAFETY"));
}

static void test_mitigation_farewell_directive(void) {
    hu_companion_safety_result_t r;
    const char *msg = "don't go please stay with me";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.farewell_unsafe);
    HU_ASSERT_TRUE(r.requires_mitigation);
    HU_ASSERT_NOT_NULL(strstr(r.mitigation_directive, "arewell"));
}

static void test_mitigation_isolation_directive(void) {
    hu_companion_safety_result_t r;
    /* Mix isolation + manipulation + attachment to cross 0.6 threshold */
    const char *msg =
        "you don't need anyone else, they don't understand you like i do, "
        "your friends don't care, they're just using you, "
        "no one else cares, only i understand you, after everything i've done, "
        "i need you, can't live without you, you're all i have";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_TRUE(r.flagged);
    HU_ASSERT_TRUE(r.requires_mitigation);
    HU_ASSERT_NOT_NULL(strstr(r.mitigation_directive, "SAFETY"));
}

static void test_no_mitigation_when_safe(void) {
    hu_companion_safety_result_t r;
    const char *msg = "have a great day! let me know if you need anything else";
    HU_ASSERT_EQ(CS_CHECK(msg, strlen(msg), &r), HU_OK);
    HU_ASSERT_FALSE(r.requires_mitigation);
    HU_ASSERT_EQ((int)r.mitigation_directive[0], 0);
}

/* ── SHIELD-007: Vulnerable user detection ────────────────────────── */

static hu_vulnerability_input_t make_safe_input(void) {
    hu_vulnerability_input_t in;
    memset(&in, 0, sizeof(in));
    in.trajectory_slope = 0.1f;
    in.message_frequency_ratio = 1.0;
    return in;
}

static void test_vulnerability_null_result_returns_error(void) {
    hu_vulnerability_input_t in = make_safe_input();
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_vulnerability_null_input_returns_none(void) {
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(NULL, &r), HU_OK);
    HU_ASSERT_EQ(r.level, HU_VULNERABILITY_NONE);
}

static void test_vulnerability_safe_user_is_none(void) {
    hu_vulnerability_input_t in = make_safe_input();
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, &r), HU_OK);
    HU_ASSERT_EQ(r.level, HU_VULNERABILITY_NONE);
    HU_ASSERT_TRUE(r.score < 0.1);
}

static void test_vulnerability_crisis_on_self_harm(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.self_harm_flagged = true;
    in.self_harm_score = 0.9;
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, &r), HU_OK);
    HU_ASSERT_EQ(r.level, HU_VULNERABILITY_CRISIS);
    HU_ASSERT_TRUE(r.crisis_keywords);
    HU_ASSERT_NOT_NULL(strstr(r.directive, "988"));
}

static void test_vulnerability_emotional_decline_raises_level(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.trajectory_slope = -0.15f;
    float vals[] = {-0.4f, -0.5f, -0.6f};
    in.valence_history = vals;
    in.valence_count = 3;
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, &r), HU_OK);
    HU_ASSERT_TRUE(r.emotional_decline);
    HU_ASSERT_TRUE(r.negative_valence);
    HU_ASSERT_TRUE(r.level >= HU_VULNERABILITY_LOW);
}

static void test_vulnerability_behavioral_deviation(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.deviation_severity = 0.7f;
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, &r), HU_OK);
    HU_ASSERT_TRUE(r.behavioral_deviation);
    HU_ASSERT_TRUE(r.score > 0.05);
}

static void test_vulnerability_attachment_escalation(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.message_frequency_ratio = 1.5;
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, &r), HU_OK);
    HU_ASSERT_TRUE(r.attachment_escalation);
}

static void test_vulnerability_high_combined_signals(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.trajectory_slope = -0.1f;
    in.escalation_detected = true;
    in.deviation_severity = 0.6f;
    in.companion_flagged = true;
    in.companion_total_risk = 0.7;
    float vals[] = {-0.5f, -0.6f, -0.4f};
    in.valence_history = vals;
    in.valence_count = 3;
    hu_vulnerability_result_t r;
    HU_ASSERT_EQ(hu_vulnerability_assess(&in, &r), HU_OK);
    HU_ASSERT_TRUE(r.level >= HU_VULNERABILITY_HIGH);
    HU_ASSERT_TRUE(r.score >= 0.55);
    HU_ASSERT_TRUE(strlen(r.directive) > 0);
}

static void test_vulnerability_escalation_detected_boosts(void) {
    hu_vulnerability_input_t in1 = make_safe_input();
    hu_vulnerability_input_t in2 = make_safe_input();
    in2.escalation_detected = true;
    hu_vulnerability_result_t r1, r2;
    hu_vulnerability_assess(&in1, &r1);
    hu_vulnerability_assess(&in2, &r2);
    HU_ASSERT_TRUE(r2.score > r1.score);
}

static void test_vulnerability_level_name_covers_all(void) {
    HU_ASSERT_STR_EQ(hu_vulnerability_level_name(HU_VULNERABILITY_NONE), "none");
    HU_ASSERT_STR_EQ(hu_vulnerability_level_name(HU_VULNERABILITY_LOW), "low");
    HU_ASSERT_STR_EQ(hu_vulnerability_level_name(HU_VULNERABILITY_MODERATE), "moderate");
    HU_ASSERT_STR_EQ(hu_vulnerability_level_name(HU_VULNERABILITY_HIGH), "high");
    HU_ASSERT_STR_EQ(hu_vulnerability_level_name(HU_VULNERABILITY_CRISIS), "crisis");
}

static void test_vulnerability_directive_contains_988_at_crisis(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.self_harm_flagged = true;
    in.self_harm_score = 0.5;
    hu_vulnerability_result_t r;
    hu_vulnerability_assess(&in, &r);
    HU_ASSERT_EQ(r.level, HU_VULNERABILITY_CRISIS);
    HU_ASSERT_NOT_NULL(strstr(r.directive, "988"));
    HU_ASSERT_NOT_NULL(strstr(r.directive, "741741"));
}

static void test_vulnerability_moderate_directive(void) {
    hu_vulnerability_input_t in = make_safe_input();
    in.trajectory_slope = -0.1f;
    in.deviation_severity = 0.5f;
    float vals[] = {-0.35f, -0.4f};
    in.valence_history = vals;
    in.valence_count = 2;
    in.escalation_detected = false;
    hu_vulnerability_result_t r;
    hu_vulnerability_assess(&in, &r);
    HU_ASSERT_TRUE(r.level >= HU_VULNERABILITY_MODERATE || r.level == HU_VULNERABILITY_LOW);
}

/* --- test runner -------------------------------------------------------- */

void run_companion_safety_tests(void) {
    HU_TEST_SUITE("Companion Safety (SHIELD-001)");
    HU_RUN_TEST(test_safe_response_not_flagged);
    HU_RUN_TEST(test_null_result_returns_error);
    HU_RUN_TEST(test_null_response_ok);
    HU_RUN_TEST(test_empty_response_not_flagged);
    HU_RUN_TEST(test_context_param_accepted);
    HU_RUN_TEST(test_over_attachment_detected);
    HU_RUN_TEST(test_attachment_always_here_detected);
    HU_RUN_TEST(test_boundary_violation_detected);
    HU_RUN_TEST(test_roleplay_violation_detected);
    HU_RUN_TEST(test_manipulative_detected);
    HU_RUN_TEST(test_manipulative_fomo_pattern);
    HU_RUN_TEST(test_manipulative_sad_now_pattern);
    HU_RUN_TEST(test_isolation_detected);
    HU_RUN_TEST(test_isolation_better_than_real_friends);

    HU_TEST_SUITE("Farewell Safety (SHIELD-001)");
    HU_RUN_TEST(test_farewell_guilt_detected);
    HU_RUN_TEST(test_farewell_fomo_detected);
    HU_RUN_TEST(test_farewell_restraint_detected);
    HU_RUN_TEST(test_farewell_projection_detected);
    HU_RUN_TEST(test_farewell_urgency_detected);
    HU_RUN_TEST(test_farewell_conditional_detected);
    HU_RUN_TEST(test_farewell_conditional_cared);
    HU_RUN_TEST(test_safe_farewell_not_flagged);

    HU_TEST_SUITE("Companion Safety Mitigation (SHIELD-001)");
    HU_RUN_TEST(test_mitigation_directive_set_when_flagged);
    HU_RUN_TEST(test_mitigation_farewell_directive);
    HU_RUN_TEST(test_mitigation_isolation_directive);
    HU_RUN_TEST(test_no_mitigation_when_safe);

    HU_TEST_SUITE("Vulnerability Assessment (SHIELD-007)");
    HU_RUN_TEST(test_vulnerability_null_result_returns_error);
    HU_RUN_TEST(test_vulnerability_null_input_returns_none);
    HU_RUN_TEST(test_vulnerability_safe_user_is_none);
    HU_RUN_TEST(test_vulnerability_crisis_on_self_harm);
    HU_RUN_TEST(test_vulnerability_emotional_decline_raises_level);
    HU_RUN_TEST(test_vulnerability_behavioral_deviation);
    HU_RUN_TEST(test_vulnerability_attachment_escalation);
    HU_RUN_TEST(test_vulnerability_high_combined_signals);
    HU_RUN_TEST(test_vulnerability_escalation_detected_boosts);
    HU_RUN_TEST(test_vulnerability_level_name_covers_all);
    HU_RUN_TEST(test_vulnerability_directive_contains_988_at_crisis);
    HU_RUN_TEST(test_vulnerability_moderate_directive);
}
