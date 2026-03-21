#include "human/cognition/emotional.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <math.h>
#include <string.h>

static hu_allocator_t alloc;

static void setup(void) {
    alloc = hu_system_allocator();
}

static void init_zeroes_all_fields(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    HU_ASSERT_EQ(ec.state.valence, 0.0f);
    HU_ASSERT_EQ(ec.state.intensity, 0.0f);
    HU_ASSERT_TRUE(!ec.state.concerning);
    HU_ASSERT_STR_EQ(ec.state.dominant_emotion, "neutral");
    HU_ASSERT_EQ(ec.source_mask, 0);
    HU_ASSERT_EQ(ec.confidence, 0.0f);
    HU_ASSERT_EQ(ec.valence_count, (size_t)0);
}

static void perceive_with_no_sources_stays_neutral(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    hu_emotional_perception_t p;
    memset(&p, 0, sizeof(p));
    p.voice_valence = NAN;
    hu_emotional_cognition_perceive(&ec, &p);
    HU_ASSERT_EQ(ec.source_mask, 0);
    HU_ASSERT_EQ(ec.state.intensity, 0.0f);
}

static void perceive_fast_capture_only(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    hu_emotional_state_t fc = {.valence = -0.6f, .intensity = 0.8f,
                                .concerning = true, .dominant_emotion = "sadness"};
    hu_emotional_perception_t p;
    memset(&p, 0, sizeof(p));
    p.fast_capture = &fc;
    p.voice_valence = NAN;
    hu_emotional_cognition_perceive(&ec, &p);

    HU_ASSERT_TRUE(ec.source_mask & HU_EMOTION_SRC_FAST_CAPTURE);
    HU_ASSERT_TRUE(ec.state.valence < 0.0f);
    HU_ASSERT_TRUE(ec.state.intensity > 0.5f);
    HU_ASSERT_TRUE(ec.state.concerning);
    HU_ASSERT_TRUE(ec.needs_empathy_boost);
}

static void perceive_multiple_sources_increases_confidence(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    hu_emotional_state_t fc = {.valence = 0.7f, .intensity = 0.6f,
                                .concerning = false, .dominant_emotion = "joy"};
    hu_emotional_state_t conv = {.valence = 0.5f, .intensity = 0.5f,
                                  .concerning = false, .dominant_emotion = "joy"};
    hu_emotional_perception_t p;
    memset(&p, 0, sizeof(p));
    p.fast_capture = &fc;
    p.conversation = &conv;
    p.voice_valence = NAN;

    hu_emotional_cognition_perceive(&ec, &p);

    float two_source_conf = ec.confidence;

    /* Now add a third source */
    hu_emotional_cognition_init(&ec);
    hu_stm_emotion_t stm_emos[] = {{.tag = HU_EMOTION_JOY, .intensity = 0.7}};
    p.stm_emotions = stm_emos;
    p.stm_emotion_count = 1;

    hu_emotional_cognition_perceive(&ec, &p);
    HU_ASSERT_TRUE(ec.confidence > two_source_conf);
}

static void trajectory_slope_reflects_trend(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);

    /* Rising valences */
    for (int i = 0; i < 5; i++) {
        hu_emotional_cognition_update_trajectory(&ec, 0.1f * (float)i);
    }
    HU_ASSERT_TRUE(ec.trajectory_slope > 0.0f);

    /* Declining valences */
    hu_emotional_cognition_init(&ec);
    for (int i = 0; i < 5; i++) {
        hu_emotional_cognition_update_trajectory(&ec, 1.0f - 0.2f * (float)i);
    }
    HU_ASSERT_TRUE(ec.trajectory_slope < 0.0f);
}

static void build_prompt_empty_when_no_signal(void) {
    setup();
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_emotional_cognition_build_prompt(&alloc, &ec, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out == NULL);
    HU_ASSERT_EQ(out_len, (size_t)0);
}

static void build_prompt_includes_emotion_when_present(void) {
    setup();
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    hu_emotional_state_t fc = {.valence = -0.5f, .intensity = 0.7f,
                                .concerning = true, .dominant_emotion = "frustration"};
    hu_emotional_perception_t p;
    memset(&p, 0, sizeof(p));
    p.fast_capture = &fc;
    p.voice_valence = NAN;
    hu_emotional_cognition_perceive(&ec, &p);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_emotional_cognition_build_prompt(&alloc, &ec, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(out, "Emotional Context") != NULL);
    HU_ASSERT_TRUE(strstr(out, "frustration") != NULL);
    HU_ASSERT_TRUE(strstr(out, "empathy") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void escalation_detected_when_negative_high_intensity(void) {
    hu_emotional_cognition_t ec;
    hu_emotional_cognition_init(&ec);
    hu_emotional_state_t fc = {.valence = -0.8f, .intensity = 0.9f,
                                .concerning = true, .dominant_emotion = "anger"};
    hu_emotional_perception_t p;
    memset(&p, 0, sizeof(p));
    p.fast_capture = &fc;
    p.voice_valence = NAN;
    hu_emotional_cognition_perceive(&ec, &p);
    HU_ASSERT_TRUE(ec.escalation_detected);
}

static void null_args_handled_gracefully(void) {
    hu_emotional_cognition_init(NULL);
    hu_emotional_cognition_perceive(NULL, NULL);
    hu_emotional_cognition_update_trajectory(NULL, 0.0f);

    hu_error_t err = hu_emotional_cognition_build_prompt(NULL, NULL, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

void run_emotional_cognition_tests(void) {
    HU_TEST_SUITE("EmotionalCognition");
    HU_RUN_TEST(init_zeroes_all_fields);
    HU_RUN_TEST(perceive_with_no_sources_stays_neutral);
    HU_RUN_TEST(perceive_fast_capture_only);
    HU_RUN_TEST(perceive_multiple_sources_increases_confidence);
    HU_RUN_TEST(trajectory_slope_reflects_trend);
    HU_RUN_TEST(build_prompt_empty_when_no_signal);
    HU_RUN_TEST(build_prompt_includes_emotion_when_present);
    HU_RUN_TEST(escalation_detected_when_negative_high_intensity);
    HU_RUN_TEST(null_args_handled_gracefully);
}
