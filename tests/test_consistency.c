#include "human/core/error.h"
#include "human/eval/consistency.h"
#include "test_framework.h"
#include <string.h>

static void consistency_prompt_alignment_matching_traits_high_score(void) {
    const char *resp = "I try to stay friendly and empathetic in every reply.";
    const char *traits[] = {"friendly", "empathetic"};
    float score = 0.0f;
    HU_ASSERT_EQ(hu_consistency_score_prompt_alignment(
                     resp, strlen(resp), traits, 2, NULL, 0, NULL, 0, &score),
                 HU_OK);
    HU_ASSERT_TRUE(score > 0.0f);
}

static void consistency_score_line_identical_is_high(void) {
    const char *line = "Same wording and tone as before for consistency.";
    float score = 0.0f;
    HU_ASSERT_EQ(hu_consistency_score_line(line, strlen(line), line, strlen(line), &score),
                 HU_OK);
    HU_ASSERT_FLOAT_EQ(score, 1.0f, 1e-4);
}

static void consistency_score_qa_different_answers_lower(void) {
    const char *a = "The capital of France is Paris.";
    const char *b = "I would suggest baking at three hundred fifty degrees.";
    float score = 0.0f;
    HU_ASSERT_EQ(hu_consistency_score_qa(a, strlen(a), b, strlen(b), &score), HU_OK);
    HU_ASSERT_TRUE(score < 1.0f);
}

static void consistency_lexical_preferred_words_boost_score(void) {
    const char *resp = "Happy to help; glad we could sort this out together.";
    const char *pref[] = {"glad", "happy"};
    float score = 0.0f;
    HU_ASSERT_EQ(hu_consistency_score_lexical(resp, strlen(resp), pref, 2, NULL, 0, &score),
                 HU_OK);
    HU_ASSERT_TRUE(score > 0.0f);
}

static void consistency_composite_all_zeros_is_zero(void) {
    hu_consistency_metrics_t m;
    memset(&m, 0, sizeof(m));
    HU_ASSERT_FLOAT_EQ(hu_consistency_composite(&m), 0.0f, 1e-6f);
}

static void drift_detector_init_no_drift(void) {
    hu_drift_detector_t d;
    hu_consistency_metrics_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.prompt_alignment = 0.5f;
    hu_drift_detector_init(&d, 0.2f);
    HU_ASSERT_FALSE(hu_drift_detector_update(&d, &obs));
}

static void drift_detector_baseline_then_drift(void) {
    hu_drift_detector_t d;
    hu_consistency_metrics_t baseline;
    hu_consistency_metrics_t drifted;
    memset(&baseline, 0, sizeof(baseline));
    memset(&drifted, 0, sizeof(drifted));
    drifted.prompt_alignment = 1.0f;
    drifted.line_consistency = 1.0f;
    drifted.qa_stability = 1.0f;
    drifted.lexical_fidelity = 1.0f;

    hu_drift_detector_init(&d, 0.1f);
    hu_drift_detector_set_baseline(&d, &baseline);
    HU_ASSERT_TRUE(hu_drift_detector_update(&d, &drifted));
}

void run_consistency_tests(void) {
    HU_TEST_SUITE("consistency");
    HU_RUN_TEST(consistency_prompt_alignment_matching_traits_high_score);
    HU_RUN_TEST(consistency_score_line_identical_is_high);
    HU_RUN_TEST(consistency_score_qa_different_answers_lower);
    HU_RUN_TEST(consistency_lexical_preferred_words_boost_score);
    HU_RUN_TEST(consistency_composite_all_zeros_is_zero);
    HU_RUN_TEST(drift_detector_init_no_drift);
    HU_RUN_TEST(drift_detector_baseline_then_drift);
}
