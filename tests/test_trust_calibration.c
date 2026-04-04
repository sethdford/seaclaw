#include "human/cognition/trust.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void tcal_init_sets_unknown_level(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    HU_ASSERT_EQ((int)state.level, (int)HU_TCAL_UNKNOWN);
    /* Init uses a neutral 0.5 baseline for dimensions and composite (EMA center). */
    HU_ASSERT_FLOAT_EQ(state.composite, 0.5f, 0.001f);
}

static void tcal_update_increases_interaction_count(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    hu_tcal_update(&state, 0.5f, 0.5f, 0.5f);
    hu_tcal_update(&state, 0.5f, 0.5f, 0.5f);
    hu_tcal_update(&state, 0.5f, 0.5f, 0.5f);
    HU_ASSERT_EQ((long long)state.interaction_count, 3LL);
}

static void tcal_update_positive_signals_raise_composite(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    float before = state.composite;
    hu_tcal_update(&state, 0.8f, 0.8f, 0.8f);
    HU_ASSERT(state.composite > before);
}

static void tcal_compute_level_zero_dims_returns_unknown(void) {
    hu_tcal_dimensions_t dims;
    memset(&dims, 0, sizeof(dims));
    HU_ASSERT_EQ((int)hu_tcal_compute_level(&dims), (int)HU_TCAL_UNKNOWN);
}

static void tcal_compute_level_high_dims_returns_established_or_deep(void) {
    hu_tcal_dimensions_t dims = {.competence = 1.0f,
                                 .benevolence = 1.0f,
                                 .integrity = 1.0f,
                                 .predictability = 1.0f,
                                 .transparency = 1.0f};
    hu_tcal_level_t lvl = hu_tcal_compute_level(&dims);
    HU_ASSERT_TRUE(lvl >= HU_TCAL_ESTABLISHED);
}

static void tcal_confidence_language_returns_non_null(void) {
    const char *a = hu_tcal_confidence_language(0.95f, HU_TCAL_UNKNOWN);
    const char *b = hu_tcal_confidence_language(0.5f, HU_TCAL_DEEP);
    const char *c = hu_tcal_confidence_language(0.0f, HU_TCAL_CAUTIOUS);
    HU_ASSERT_NOT_NULL(a);
    HU_ASSERT_NOT_NULL(b);
    HU_ASSERT_NOT_NULL(c);
    HU_ASSERT_TRUE(strlen(a) > 0);
    HU_ASSERT_TRUE(strlen(b) > 0);
    HU_ASSERT_TRUE(strlen(c) > 0);
}

static void tcal_build_context_produces_string(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_tcal_build_context(&alloc, &state, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_GT((long long)out_len, 0LL);
    HU_ASSERT_TRUE(strlen(out) == out_len);
    alloc.free(alloc.ctx, out, out_len + 1U);
}

static void tcal_check_erosion_false_on_fresh_state(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    HU_ASSERT_FALSE(hu_tcal_check_erosion(&state));
}

static void tcal_update_negative_then_check_erosion(void) {
    hu_tcal_state_t state;
    hu_tcal_init(&state);
    hu_tcal_update(&state, -1.0f, -1.0f, -1.0f);
    HU_ASSERT_TRUE(hu_tcal_check_erosion(&state));
    HU_ASSERT_TRUE(state.erosion_detected);
}

void run_trust_calibration_tests(void) {
    HU_TEST_SUITE("trust_calibration");
    HU_RUN_TEST(tcal_init_sets_unknown_level);
    HU_RUN_TEST(tcal_update_increases_interaction_count);
    HU_RUN_TEST(tcal_update_positive_signals_raise_composite);
    HU_RUN_TEST(tcal_compute_level_zero_dims_returns_unknown);
    HU_RUN_TEST(tcal_compute_level_high_dims_returns_established_or_deep);
    HU_RUN_TEST(tcal_confidence_language_returns_non_null);
    HU_RUN_TEST(tcal_build_context_produces_string);
    HU_RUN_TEST(tcal_check_erosion_false_on_fresh_state);
    HU_RUN_TEST(tcal_update_negative_then_check_erosion);
}
