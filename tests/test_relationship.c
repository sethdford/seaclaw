#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/persona/relationship.h"
#include "test_framework.h"
#include <string.h>

static void relationship_new_stage(void) {
    hu_relationship_state_t state = {0};
    HU_ASSERT_EQ(state.stage, HU_REL_NEW);
    hu_relationship_new_session(&state);
    HU_ASSERT_EQ(state.stage, HU_REL_NEW);
    HU_ASSERT_EQ(state.session_count, 1u);
}

static void relationship_familiar_after_5(void) {
    hu_relationship_state_t state = {0};
    for (int i = 0; i < 5; i++)
        hu_relationship_new_session(&state);
    HU_ASSERT_EQ(state.stage, HU_REL_FAMILIAR);
    HU_ASSERT_EQ(state.session_count, 5u);
}

static void relationship_trusted_after_20(void) {
    hu_relationship_state_t state = {0};
    for (int i = 0; i < 20; i++)
        hu_relationship_new_session(&state);
    HU_ASSERT_EQ(state.stage, HU_REL_TRUSTED);
    HU_ASSERT_EQ(state.session_count, 20u);
}

static void relationship_deep_after_50(void) {
    hu_relationship_state_t state = {0};
    for (int i = 0; i < 50; i++)
        hu_relationship_new_session(&state);
    HU_ASSERT_EQ(state.stage, HU_REL_DEEP);
    HU_ASSERT_EQ(state.session_count, 50u);
}

static void relationship_update_increments_turns_not_sessions(void) {
    hu_relationship_state_t state = {0};
    hu_relationship_new_session(&state);
    HU_ASSERT_EQ(state.session_count, 1u);
    HU_ASSERT_EQ(state.total_turns, 0u);
    hu_relationship_update(&state, 1);
    HU_ASSERT_EQ(state.session_count, 1u);
    HU_ASSERT_EQ(state.total_turns, 1u);
    hu_relationship_update(&state, 3);
    HU_ASSERT_EQ(state.session_count, 1u);
    HU_ASSERT_EQ(state.total_turns, 4u);
}

static void relationship_build_prompt_contains_stage(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_relationship_state_t state = {
        .stage = HU_REL_FAMILIAR, .session_count = 10, .total_turns = 25};
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_relationship_build_prompt(&alloc, &state, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Relationship Context") != NULL);
    HU_ASSERT_TRUE(strstr(out, "familiar") != NULL);
    HU_ASSERT_TRUE(strstr(out, "Sessions: 10") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void relationship_new_session_null_safe(void) {
    hu_relationship_new_session(NULL);
}

static void relationship_build_prompt_null_state_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_relationship_build_prompt(&alloc, NULL, &out, &out_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(out);
}

/* --- Quality-weighted progression tests --- */

static void relationship_quality_score_weighted_composite(void) {
    hu_session_quality_t q = {
        .emotional_exchanges = 1.0f,
        .topic_diversity = 1.0f,
        .vulnerability_events = 1.0f,
        .humor_shared = 1.0f,
        .repair_survived = 1.0f,
    };
    float score = hu_session_quality_score(&q);
    /* 1.0*0.3 + 1.0*0.2 + 1.0*0.3 + 1.0*0.1 + 1.0*0.1 = 1.0 */
    HU_ASSERT_TRUE(score >= 0.99f && score <= 1.01f);
}

static void relationship_quality_score_null_returns_zero(void) {
    float score = hu_session_quality_score(NULL);
    HU_ASSERT_TRUE(score == 0.0f);
}

static void relationship_quality_score_partial_signals(void) {
    hu_session_quality_t q = {
        .emotional_exchanges = 0.5f,
        .topic_diversity = 0.0f,
        .vulnerability_events = 0.0f,
        .humor_shared = 0.0f,
        .repair_survived = 0.0f,
    };
    float score = hu_session_quality_score(&q);
    /* 0.5*0.3 = 0.15 */
    HU_ASSERT_TRUE(score >= 0.14f && score <= 0.16f);
}

static void relationship_quality_accumulation_advances_stage(void) {
    hu_relationship_state_t state = {0};
    hu_session_quality_t high = {
        .emotional_exchanges = 0.9f,
        .topic_diversity = 0.8f,
        .vulnerability_events = 0.9f,
        .humor_shared = 0.7f,
        .repair_survived = 0.5f,
    };
    /* High quality sessions should advance toward FAMILIAR */
    for (int i = 0; i < 10; i++)
        hu_relationship_new_session_quality(&state, &high, 1.0f);
    HU_ASSERT_TRUE(state.stage >= HU_REL_FAMILIAR);
    HU_ASSERT_EQ(state.session_count, 10u);
}

static void relationship_quality_low_sessions_stay_new(void) {
    hu_relationship_state_t state = {0};
    hu_session_quality_t low = {
        .emotional_exchanges = 0.1f,
        .topic_diversity = 0.1f,
        .vulnerability_events = 0.0f,
        .humor_shared = 0.0f,
        .repair_survived = 0.0f,
    };
    /* Low quality: 0.1*0.3 + 0.1*0.2 = 0.05 per session */
    for (int i = 0; i < 5; i++)
        hu_relationship_new_session_quality(&state, &low, 1.0f);
    HU_ASSERT_EQ(state.stage, HU_REL_NEW);
}

static void relationship_quality_regression_on_drop(void) {
    hu_relationship_state_t state = {0};
    hu_session_quality_t high = {
        .emotional_exchanges = 1.0f,
        .topic_diversity = 1.0f,
        .vulnerability_events = 1.0f,
        .humor_shared = 1.0f,
        .repair_survived = 1.0f,
    };
    /* Build up to FAMILIAR+ */
    for (int i = 0; i < 15; i++)
        hu_relationship_new_session_quality(&state, &high, 1.0f);
    hu_relationship_stage_t peak = state.stage;
    HU_ASSERT_TRUE(peak >= HU_REL_FAMILIAR);

    /* Now send many zero-quality sessions — should regress */
    hu_session_quality_t zero = {0};
    for (int i = 0; i < 30; i++)
        hu_relationship_new_session_quality(&state, &zero, 1.0f);
    HU_ASSERT_TRUE(state.stage < peak);
}

static void relationship_quality_velocity_accelerates(void) {
    hu_relationship_state_t fast = {0};
    hu_relationship_state_t slow = {0};
    hu_session_quality_t q = {
        .emotional_exchanges = 0.7f,
        .topic_diversity = 0.5f,
        .vulnerability_events = 0.6f,
        .humor_shared = 0.4f,
        .repair_survived = 0.3f,
    };
    for (int i = 0; i < 10; i++) {
        hu_relationship_new_session_quality(&fast, &q, 2.0f); /* 2x velocity */
        hu_relationship_new_session_quality(&slow, &q, 1.0f); /* normal */
    }
    HU_ASSERT_TRUE(fast.quality.cumulative_quality >= slow.quality.cumulative_quality);
}

static void relationship_quality_null_safe(void) {
    hu_relationship_state_t state = {0};
    hu_session_quality_t q = {.emotional_exchanges = 0.5f};
    hu_relationship_new_session_quality(NULL, &q, 1.0f);
    hu_relationship_new_session_quality(&state, NULL, 1.0f);
    HU_ASSERT_EQ(state.session_count, 0u);
}

static void relationship_quality_session_count_alone_not_enough(void) {
    hu_relationship_state_t state = {0};
    hu_session_quality_t zero = {0};
    /* 100 zero-quality sessions should not reach DEEP */
    for (int i = 0; i < 100; i++)
        hu_relationship_new_session_quality(&state, &zero, 1.0f);
    HU_ASSERT_TRUE(state.stage < HU_REL_DEEP);
}

void run_relationship_tests(void) {
    HU_TEST_SUITE("relationship");
    HU_RUN_TEST(relationship_new_stage);
    HU_RUN_TEST(relationship_familiar_after_5);
    HU_RUN_TEST(relationship_trusted_after_20);
    HU_RUN_TEST(relationship_deep_after_50);
    HU_RUN_TEST(relationship_update_increments_turns_not_sessions);
    HU_RUN_TEST(relationship_build_prompt_contains_stage);
    HU_RUN_TEST(relationship_new_session_null_safe);
    HU_RUN_TEST(relationship_build_prompt_null_state_fails);
    /* Quality-weighted progression */
    HU_RUN_TEST(relationship_quality_score_weighted_composite);
    HU_RUN_TEST(relationship_quality_score_null_returns_zero);
    HU_RUN_TEST(relationship_quality_score_partial_signals);
    HU_RUN_TEST(relationship_quality_accumulation_advances_stage);
    HU_RUN_TEST(relationship_quality_low_sessions_stay_new);
    HU_RUN_TEST(relationship_quality_regression_on_drop);
    HU_RUN_TEST(relationship_quality_velocity_accelerates);
    HU_RUN_TEST(relationship_quality_null_safe);
    HU_RUN_TEST(relationship_quality_session_count_alone_not_enough);
}
