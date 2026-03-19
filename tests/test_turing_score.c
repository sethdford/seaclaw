#include "test_framework.h"
#include "human/eval/turing_score.h"

static void turing_heuristic_casual_message_scores_high(void) {
    hu_turing_score_t score;
    const char *msg = "haha yeah that sounds fun, i'm down";
    HU_ASSERT(hu_turing_score_heuristic(msg, strlen(msg), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.overall >= 7);
    HU_ASSERT(score.dimensions[HU_TURING_NATURAL_LANGUAGE] >= 7);
    HU_ASSERT(score.dimensions[HU_TURING_IMPERFECTION] >= 7);
    HU_ASSERT(score.verdict == HU_TURING_HUMAN || score.verdict == HU_TURING_BORDERLINE);
}

static void turing_heuristic_ai_tells_score_low(void) {
    hu_turing_score_t score;
    const char *msg = "I'd be happy to help you with that! I certainly understand your concern. "
                      "Feel free to reach out anytime.";
    HU_ASSERT(hu_turing_score_heuristic(msg, strlen(msg), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_NON_ROBOTIC] <= 5);
    HU_ASSERT(score.dimensions[HU_TURING_NATURAL_LANGUAGE] <= 6);
}

static void turing_heuristic_markdown_penalized(void) {
    hu_turing_score_t score;
    const char *msg = "Here are some options:\n- Option 1\n- Option 2\n- Option 3";
    HU_ASSERT(hu_turing_score_heuristic(msg, strlen(msg), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_NON_ROBOTIC] < 8);
}

static void turing_heuristic_emotional_message(void) {
    hu_turing_score_t score;
    const char *msg = "i miss you so much, honestly i'm just sad today";
    HU_ASSERT(hu_turing_score_heuristic(msg, strlen(msg), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_EMOTIONAL_INTELLIGENCE] >= 7);
    HU_ASSERT(score.dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] >= 7);
    HU_ASSERT(score.dimensions[HU_TURING_GENUINE_WARMTH] >= 7);
}

static void turing_heuristic_long_message_penalized(void) {
    hu_turing_score_t score;
    char long_msg[500];
    memset(long_msg, 'a', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';
    HU_ASSERT(hu_turing_score_heuristic(long_msg, strlen(long_msg), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_APPROPRIATE_LENGTH] < 5);
}

static void turing_heuristic_hedging_penalized(void) {
    hu_turing_score_t score;
    const char *msg = "Well, it depends on the situation. On one hand you could do X.";
    HU_ASSERT(hu_turing_score_heuristic(msg, strlen(msg), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_OPINION_HAVING] <= 5);
}

static void turing_dimension_names_valid(void) {
    HU_ASSERT(strcmp(hu_turing_dimension_name(HU_TURING_NATURAL_LANGUAGE),
                     "natural_language") == 0);
    HU_ASSERT(strcmp(hu_turing_dimension_name(HU_TURING_GENUINE_WARMTH),
                     "genuine_warmth") == 0);
}

static void turing_verdict_names_valid(void) {
    HU_ASSERT(strcmp(hu_turing_verdict_name(HU_TURING_HUMAN), "HUMAN") == 0);
    HU_ASSERT(strcmp(hu_turing_verdict_name(HU_TURING_AI_DETECTED), "AI_DETECTED") == 0);
}

static void turing_score_summary_allocates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_turing_score_t score;
    const char *msg = "yeah for sure";
    HU_ASSERT(hu_turing_score_heuristic(msg, strlen(msg), NULL, 0, &score) == HU_OK);

    size_t len = 0;
    char *summary = hu_turing_score_summary(&alloc, &score, &len);
    HU_ASSERT(summary != NULL);
    HU_ASSERT(len > 0);
    HU_ASSERT(strstr(summary, "Turing Score:") != NULL);
    alloc.free(alloc.ctx, summary, len + 1);
}

static void turing_null_input_rejected(void) {
    hu_turing_score_t score;
    HU_ASSERT(hu_turing_score_heuristic(NULL, 0, NULL, 0, &score) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_turing_score_heuristic("hi", 2, NULL, 0, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void turing_llm_null_provider_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_turing_score_t score;
    HU_ASSERT(hu_turing_score_llm(&alloc, NULL, "m", 1, "hi", 2, NULL, 0, &score) ==
              HU_ERR_INVALID_ARGUMENT);
}

static void turing_scores_clamped(void) {
    hu_turing_score_t score;
    const char *msg = "x";
    HU_ASSERT(hu_turing_score_heuristic(msg, 1, NULL, 0, &score) == HU_OK);
    for (int i = 0; i < HU_TURING_DIM_COUNT; i++) {
        HU_ASSERT(score.dimensions[i] >= 1);
        HU_ASSERT(score.dimensions[i] <= 10);
    }
}

void run_turing_score_tests(void) {
    HU_RUN_TEST(turing_heuristic_casual_message_scores_high);
    HU_RUN_TEST(turing_heuristic_ai_tells_score_low);
    HU_RUN_TEST(turing_heuristic_markdown_penalized);
    HU_RUN_TEST(turing_heuristic_emotional_message);
    HU_RUN_TEST(turing_heuristic_long_message_penalized);
    HU_RUN_TEST(turing_heuristic_hedging_penalized);
    HU_RUN_TEST(turing_dimension_names_valid);
    HU_RUN_TEST(turing_verdict_names_valid);
    HU_RUN_TEST(turing_score_summary_allocates);
    HU_RUN_TEST(turing_null_input_rejected);
    HU_RUN_TEST(turing_llm_null_provider_rejected);
    HU_RUN_TEST(turing_scores_clamped);
}
