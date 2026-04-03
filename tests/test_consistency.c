#include "test_framework.h"
#include "human/eval/consistency.h"
#include <string.h>

/* ── Prompt alignment ────────────────────────────────────────────── */

static void prompt_alignment_matching_traits(void) {
    const char *traits[] = {"friendly", "helpful"};
    const char *preferred[] = {"great", "awesome"};
    float score = 0.0f;
    const char *resp = "That's a great question! I'm happy to be helpful and friendly.";
    HU_ASSERT(hu_consistency_score_prompt_alignment(
        resp, strlen(resp), traits, 2, preferred, 2, NULL, 0, &score) == HU_OK);
    HU_ASSERT(score > 0.3f);
}

static void prompt_alignment_no_traits_match(void) {
    const char *traits[] = {"sarcastic", "terse"};
    float score = 0.0f;
    const char *resp = "Sure, I can look into that for you right away!";
    HU_ASSERT(hu_consistency_score_prompt_alignment(
        resp, strlen(resp), traits, 2, NULL, 0, NULL, 0, &score) == HU_OK);
    HU_ASSERT(score < 0.5f);
}

static void prompt_alignment_avoided_words_penalize(void) {
    const char *avoided[] = {"absolutely", "definitely"};
    float score = 0.0f;
    const char *resp = "I absolutely and definitely agree with everything.";
    HU_ASSERT(hu_consistency_score_prompt_alignment(
        resp, strlen(resp), NULL, 0, NULL, 0, avoided, 2, &score) == HU_OK);
    HU_ASSERT(score < 0.9f);
}

static void prompt_alignment_null_fails(void) {
    float score = 0.0f;
    HU_ASSERT(hu_consistency_score_prompt_alignment(
        NULL, 0, NULL, 0, NULL, 0, NULL, 0, &score) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Line-to-line consistency ────────────────────────────────────── */

static void line_consistency_similar_responses(void) {
    float score = 0.0f;
    const char *a = "I think that's a good idea, let me help you with that.";
    const char *b = "That sounds like a good idea, I can definitely help.";
    HU_ASSERT(hu_consistency_score_line(a, strlen(a), b, strlen(b), &score) == HU_OK);
    HU_ASSERT(score > 0.2f);
}

static void line_consistency_different_responses(void) {
    float score = 0.0f;
    const char *a = "Yes.";
    const char *b = "Well, I think there are many nuanced perspectives to consider here, and we should examine each one carefully.";
    HU_ASSERT(hu_consistency_score_line(a, strlen(a), b, strlen(b), &score) == HU_OK);
    HU_ASSERT(score < 0.5f);
}

static void line_consistency_null_fails(void) {
    float score = 0.0f;
    HU_ASSERT(hu_consistency_score_line(NULL, 0, "x", 1, &score) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Q&A stability ───────────────────────────────────────────────── */

static void qa_stability_identical_answers(void) {
    float score = 0.0f;
    const char *a = "My favorite color is blue.";
    HU_ASSERT(hu_consistency_score_qa(a, strlen(a), a, strlen(a), &score) == HU_OK);
    HU_ASSERT(score > 0.9f);
}

static void qa_stability_different_answers(void) {
    float score = 0.0f;
    const char *a = "My favorite color is blue.";
    const char *b = "I absolutely love red, it's vibrant and energizing.";
    HU_ASSERT(hu_consistency_score_qa(a, strlen(a), b, strlen(b), &score) == HU_OK);
    HU_ASSERT(score < 0.5f);
}

/* ── Lexical fidelity ────────────────────────────────────────────── */

static void lexical_fidelity_preferred_present(void) {
    const char *preferred[] = {"yo", "chill"};
    float score = 0.0f;
    const char *resp = "Yo, that's pretty chill if you ask me.";
    HU_ASSERT(hu_consistency_score_lexical(
        resp, strlen(resp), preferred, 2, NULL, 0, &score) == HU_OK);
    HU_ASSERT(score > 0.8f);
}

static void lexical_fidelity_avoided_present(void) {
    const char *avoided[] = {"synergy", "leverage"};
    float score = 0.0f;
    const char *resp = "We should leverage synergy to optimize our workflow.";
    HU_ASSERT(hu_consistency_score_lexical(
        resp, strlen(resp), NULL, 0, avoided, 2, &score) == HU_OK);
    HU_ASSERT(score < 0.7f);
}

/* ── Composite ───────────────────────────────────────────────────── */

static void composite_weighted_average(void) {
    hu_consistency_metrics_t m = {
        .prompt_alignment = 0.8f,
        .line_consistency = 0.7f,
        .qa_stability = 0.9f,
        .lexical_fidelity = 0.6f,
    };
    float c = hu_consistency_composite(&m);
    HU_ASSERT(c > 0.5f && c < 1.0f);
}

static void composite_null_returns_zero(void) {
    HU_ASSERT(hu_consistency_composite(NULL) == 0.0f);
}

/* ── Drift detector ──────────────────────────────────────────────── */

static void drift_detector_no_baseline_no_drift(void) {
    hu_drift_detector_t d;
    hu_drift_detector_init(&d, 0.15f);
    hu_consistency_metrics_t m = {.prompt_alignment = 0.5f, .line_consistency = 0.5f,
                                   .qa_stability = 0.5f, .lexical_fidelity = 0.5f};
    HU_ASSERT(!hu_drift_detector_update(&d, &m));
}

static void drift_detector_detects_large_change(void) {
    hu_drift_detector_t d;
    hu_drift_detector_init(&d, 0.15f);
    hu_consistency_metrics_t baseline = {.prompt_alignment = 0.8f, .line_consistency = 0.8f,
                                          .qa_stability = 0.8f, .lexical_fidelity = 0.8f};
    hu_drift_detector_set_baseline(&d, &baseline);

    hu_consistency_metrics_t bad = {.prompt_alignment = 0.3f, .line_consistency = 0.3f,
                                     .qa_stability = 0.3f, .lexical_fidelity = 0.3f};
    HU_ASSERT(hu_drift_detector_update(&d, &bad));
    HU_ASSERT(d.drift_detected);
}

static void drift_detector_stable_no_drift(void) {
    hu_drift_detector_t d;
    hu_drift_detector_init(&d, 0.15f);
    hu_consistency_metrics_t baseline = {.prompt_alignment = 0.8f, .line_consistency = 0.8f,
                                          .qa_stability = 0.8f, .lexical_fidelity = 0.8f};
    hu_drift_detector_set_baseline(&d, &baseline);
    hu_consistency_metrics_t stable = {.prompt_alignment = 0.78f, .line_consistency = 0.82f,
                                        .qa_stability = 0.79f, .lexical_fidelity = 0.81f};
    HU_ASSERT(!hu_drift_detector_update(&d, &stable));
}

/* ── Runner ──────────────────────────────────────────────────────── */

void run_consistency_tests(void) {
    HU_TEST_SUITE("Personality Consistency");

    HU_RUN_TEST(prompt_alignment_matching_traits);
    HU_RUN_TEST(prompt_alignment_no_traits_match);
    HU_RUN_TEST(prompt_alignment_avoided_words_penalize);
    HU_RUN_TEST(prompt_alignment_null_fails);

    HU_RUN_TEST(line_consistency_similar_responses);
    HU_RUN_TEST(line_consistency_different_responses);
    HU_RUN_TEST(line_consistency_null_fails);

    HU_RUN_TEST(qa_stability_identical_answers);
    HU_RUN_TEST(qa_stability_different_answers);

    HU_RUN_TEST(lexical_fidelity_preferred_present);
    HU_RUN_TEST(lexical_fidelity_avoided_present);

    HU_RUN_TEST(composite_weighted_average);
    HU_RUN_TEST(composite_null_returns_zero);

    HU_RUN_TEST(drift_detector_no_baseline_no_drift);
    HU_RUN_TEST(drift_detector_detects_large_change);
    HU_RUN_TEST(drift_detector_stable_no_drift);
}
