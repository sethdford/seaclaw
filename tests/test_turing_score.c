#include "human/eval/turing_score.h"
#include "test_framework.h"

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
    HU_ASSERT(strcmp(hu_turing_dimension_name(HU_TURING_NATURAL_LANGUAGE), "natural_language") ==
              0);
    HU_ASSERT(strcmp(hu_turing_dimension_name(HU_TURING_GENUINE_WARMTH), "genuine_warmth") == 0);
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

static void turing_energy_matching_short_to_short(void) {
    hu_turing_score_t score;
    const char *ctx = "hey whats up\n";
    const char *resp = "not much, you?";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), ctx, strlen(ctx), &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_ENERGY_MATCHING] >= 7);
}

static void turing_energy_matching_essay_to_short_penalized(void) {
    hu_turing_score_t score;
    const char *ctx = "k\n";
    char long_resp[300];
    memset(long_resp, 'a', sizeof(long_resp) - 1);
    long_resp[sizeof(long_resp) - 1] = '\0';
    HU_ASSERT(hu_turing_score_heuristic(long_resp, strlen(long_resp), ctx, strlen(ctx), &score) ==
              HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_ENERGY_MATCHING] <= 5);
}

static void turing_context_awareness_with_references(void) {
    hu_turing_score_t score;
    const char *ctx = "I started a new job last month\n";
    const char *resp = "you mentioned your new job earlier, how's that going?";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), ctx, strlen(ctx), &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_CONTEXT_AWARENESS] >= 8);
}

static void turing_context_awareness_no_context(void) {
    hu_turing_score_t score;
    const char *resp = "that sounds cool";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_CONTEXT_AWARENESS] >= 5);
    HU_ASSERT(score.dimensions[HU_TURING_CONTEXT_AWARENESS] <= 8);
}

static void turing_vulnerability_authentic_disclosure(void) {
    hu_turing_score_t score;
    const char *resp = "honestly i feel like i've been struggling with this, ngl";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] >= 7);
}

static void turing_humor_brief_natural(void) {
    hu_turing_score_t score;
    const char *resp = "haha omg dead";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_HUMOR_NATURALNESS] >= 7);
}

static void turing_opinion_strong_position(void) {
    hu_turing_score_t score;
    const char *resp = "i think that show is overrated, personally i prefer the original";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_OPINION_HAVING] >= 8);
}

static void turing_personality_consistent_voice(void) {
    hu_turing_score_t score;
    const char *resp = "honestly i'm not sure but imo it's better to just go for it, tbh "
                       "i've been thinking about this a lot";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_PERSONALITY_CONSISTENCY] >= 7);
}

static void turing_genuine_warmth_with_context(void) {
    hu_turing_score_t score;
    const char *ctx = "started new job last week\n";
    const char *resp = "i'm so happy for you! you mentioned the new job, how's it going?";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), ctx, strlen(ctx), &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_GENUINE_WARMTH] >= 7);
}

static void turing_voice_filler_usage_detected(void) {
    hu_turing_score_t score;
    const char *resp = "well, i guess like, i dunno, it's kinda weird ya know";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_FILLER_USAGE] >= 8);
}

static void turing_voice_conversational_repair_detected(void) {
    hu_turing_score_t score;
    const char *resp = "i think it's— wait, actually, i mean it's more like, no wait, scratch that";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_CONVERSATIONAL_REPAIR] >= 8);
}

static void turing_voice_paralinguistic_cues_detected(void) {
    hu_turing_score_t score;
    const char *resp = "haha aww that's so sweet, oof i felt that";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_PARALINGUISTIC_CUES] >= 8);
}

static void turing_voice_emotional_prosody_with_emphasis(void) {
    hu_turing_score_t score;
    const char *resp = "OMG that's AMAZING!! i'm so happy for you!! i love it so much";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_EMOTIONAL_PROSODY] >= 8);
}

static void turing_voice_prosody_varied_punctuation(void) {
    hu_turing_score_t score;
    const char *resp = "wait really? that's wild! i can't... like i'm still processing";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), NULL, 0, &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_PROSODY_NATURALNESS] >= 8);
}

static void turing_voice_turn_timing_short_casual(void) {
    hu_turing_score_t score;
    const char *ctx = "hey\n";
    const char *resp = "lol yeah what's up";
    HU_ASSERT(hu_turing_score_heuristic(resp, strlen(resp), ctx, strlen(ctx), &score) == HU_OK);
    HU_ASSERT(score.dimensions[HU_TURING_TURN_TIMING] >= 7);
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
    HU_RUN_TEST(turing_energy_matching_short_to_short);
    HU_RUN_TEST(turing_energy_matching_essay_to_short_penalized);
    HU_RUN_TEST(turing_context_awareness_with_references);
    HU_RUN_TEST(turing_context_awareness_no_context);
    HU_RUN_TEST(turing_vulnerability_authentic_disclosure);
    HU_RUN_TEST(turing_humor_brief_natural);
    HU_RUN_TEST(turing_opinion_strong_position);
    HU_RUN_TEST(turing_personality_consistent_voice);
    HU_RUN_TEST(turing_genuine_warmth_with_context);
    HU_RUN_TEST(turing_voice_filler_usage_detected);
    HU_RUN_TEST(turing_voice_conversational_repair_detected);
    HU_RUN_TEST(turing_voice_paralinguistic_cues_detected);
    HU_RUN_TEST(turing_voice_emotional_prosody_with_emphasis);
    HU_RUN_TEST(turing_voice_prosody_varied_punctuation);
    HU_RUN_TEST(turing_voice_turn_timing_short_casual);
}
