#include "test_framework.h"
#include "human/cognition/trust.h"
#include "human/core/allocator.h"
#include <string.h>

static void trust_cal_init_default_values(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    HU_ASSERT(state.dimensions.competence == 0.5f);
    HU_ASSERT(state.dimensions.benevolence == 0.5f);
    HU_ASSERT(state.dimensions.integrity == 0.5f);
    HU_ASSERT(state.level == HU_TCAL_UNKNOWN);
    HU_ASSERT(state.interaction_count == 0);
    HU_ASSERT(!state.erosion_detected);
}

static void trust_cal_init_null_safe(void) {
    hu_tcal_init(NULL);
}

static void trust_cal_update_positive_signals(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    for (int i = 0; i < 20; i++)
        hu_tcal_update(&state, 0.9f, 0.9f, 0.9f);
    HU_ASSERT(state.dimensions.competence > 0.7f);
    HU_ASSERT(state.level >= HU_TCAL_DEVELOPING);
    HU_ASSERT(state.interaction_count == 20);
}

static void trust_cal_update_negative_signals_erode(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    for (int i = 0; i < 10; i++)
        hu_tcal_update(&state, 0.9f, 0.9f, 0.9f);
    float prev = state.composite;
    hu_tcal_update(&state, 0.1f, 0.1f, 0.1f);
    HU_ASSERT(state.composite < prev);
}

static void trust_cal_update_null_safe(void) {
    hu_tcal_update(NULL, 0.5f, 0.5f, 0.5f);
}

static void trust_cal_level_deep(void) {
    hu_tcal_dimensions_t dims = {.competence = 0.9f, .benevolence = 0.9f,
                                  .integrity = 0.9f, .predictability = 0.9f,
                                  .transparency = 0.9f};
    HU_ASSERT(hu_tcal_compute_level(&dims) == HU_TCAL_DEEP);
}

static void trust_cal_level_cautious(void) {
    hu_tcal_dimensions_t dims = {.competence = 0.3f, .benevolence = 0.3f,
                                  .integrity = 0.3f, .predictability = 0.3f,
                                  .transparency = 0.3f};
    HU_ASSERT(hu_tcal_compute_level(&dims) == HU_TCAL_CAUTIOUS);
}

static void trust_cal_level_null_unknown(void) {
    HU_ASSERT(hu_tcal_compute_level(NULL) == HU_TCAL_UNKNOWN);
}

static void trust_cal_confidence_language_high(void) {
    const char *lang = hu_tcal_confidence_language(0.95f, HU_TCAL_ESTABLISHED);
    HU_ASSERT(strstr(lang, "confident") != NULL);
}

static void trust_cal_confidence_language_low(void) {
    const char *lang = hu_tcal_confidence_language(0.15f, HU_TCAL_CAUTIOUS);
    HU_ASSERT(strstr(lang, "don't know") != NULL);
}

static void trust_cal_context_builds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    for (int i = 0; i < 5; i++)
        hu_tcal_update(&state, 0.8f, 0.8f, 0.8f);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_tcal_build_context(&alloc, &state, &out, &out_len) == HU_OK);
    HU_ASSERT(out != NULL);
    HU_ASSERT(strstr(out, "TRUST CALIBRATION") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void trust_cal_context_null_fails(void) {
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_tcal_build_context(NULL, NULL, &out, &out_len) == HU_ERR_INVALID_ARGUMENT);
}

static void trust_cal_erosion_null_false(void) {
    HU_ASSERT(!hu_tcal_check_erosion(NULL));
}

void run_trust_calibration_tests(void) {
    HU_TEST_SUITE("Trust Calibration SOTA");

    HU_RUN_TEST(trust_cal_init_default_values);
    HU_RUN_TEST(trust_cal_init_null_safe);
    HU_RUN_TEST(trust_cal_update_positive_signals);
    HU_RUN_TEST(trust_cal_update_negative_signals_erode);
    HU_RUN_TEST(trust_cal_update_null_safe);
    HU_RUN_TEST(trust_cal_level_deep);
    HU_RUN_TEST(trust_cal_level_cautious);
    HU_RUN_TEST(trust_cal_level_null_unknown);
    HU_RUN_TEST(trust_cal_confidence_language_high);
    HU_RUN_TEST(trust_cal_confidence_language_low);
    HU_RUN_TEST(trust_cal_context_builds);
    HU_RUN_TEST(trust_cal_context_null_fails);
    HU_RUN_TEST(trust_cal_erosion_null_false);
}
