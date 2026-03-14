#include "human/context/intelligence.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void protective_topic_is_blocked_returns_true(void) {
    hu_boundary_t b = {
        .contact_id = "c1", .contact_id_len = 2,
        .topic = "divorce", .topic_len = 7,
        .type = "avoid", .type_len = 5, .set_at = 1000
    };
    HU_ASSERT_TRUE(hu_protective_topic_is_blocked(&b, 1, "divorce", 7));
}

static void protective_topic_not_blocked_returns_false(void) {
    hu_boundary_t b = {
        .contact_id = "c1", .contact_id_len = 2,
        .topic = "divorce", .topic_len = 7,
        .type = "avoid", .type_len = 5, .set_at = 1000
    };
    HU_ASSERT_FALSE(hu_protective_topic_is_blocked(&b, 1, "pizza", 5));
}

static void humor_select_serious_topic_returns_none(void) {
    hu_humor_config_t cfg = {
        .humor_probability = 0.5,
        .never_during_crisis = true,
        .never_during_serious = true,
        .preferred = HU_HUMOR_OBSERVATIONAL
    };
    hu_humor_style_t s = hu_humor_select_style(0.8, true, false, &cfg, 42);
    HU_ASSERT_EQ(s, HU_HUMOR_NONE);
}

static void protective_topic_null_boundaries_returns_false(void) {
    HU_ASSERT_FALSE(hu_protective_topic_is_blocked(NULL, 0, "topic", 5));
}

static void humor_select_casual_returns_valid_style(void) {
    hu_humor_config_t cfg = {
        .humor_probability = 1.0,
        .never_during_crisis = true,
        .never_during_serious = true,
        .preferred = HU_HUMOR_OBSERVATIONAL
    };
    hu_humor_style_t s = hu_humor_select_style(0.8, false, false, &cfg, 42);
    HU_ASSERT_TRUE(s >= HU_HUMOR_NONE && s <= HU_HUMOR_DEADPAN);
}

static void humor_style_str_returns_non_null(void) {
    HU_ASSERT_NOT_NULL(hu_humor_style_str(HU_HUMOR_OBSERVATIONAL));
    HU_ASSERT_NOT_NULL(hu_humor_style_str(HU_HUMOR_NONE));
}

static void protective_build_prompt_with_boundary(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_boundary_t b = {
        .contact_id = "c1", .contact_id_len = 2,
        .topic = "health", .topic_len = 6,
        .type = "redirect", .type_len = 8, .set_at = 1000
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_protective_build_prompt(&alloc, &b, 1, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void cognitive_compute_load_zero_inputs(void) {
    double load = hu_cognitive_compute_load(0, 0, false);
    HU_ASSERT_TRUE(load >= 0.0 && load <= 1.0);
}

static void cognitive_compute_load_high_inputs(void) {
    double load = hu_cognitive_compute_load(10, 200, true);
    HU_ASSERT_TRUE(load >= 0.0 && load <= 1.0);
}

void run_intelligence_tests(void) {
    HU_RUN_TEST(protective_topic_is_blocked_returns_true);
    HU_RUN_TEST(protective_topic_not_blocked_returns_false);
    HU_RUN_TEST(humor_select_serious_topic_returns_none);
    HU_RUN_TEST(protective_topic_null_boundaries_returns_false);
    HU_RUN_TEST(humor_select_casual_returns_valid_style);
    HU_RUN_TEST(humor_style_str_returns_non_null);
    HU_RUN_TEST(protective_build_prompt_with_boundary);
    HU_RUN_TEST(cognitive_compute_load_zero_inputs);
    HU_RUN_TEST(cognitive_compute_load_high_inputs);
}
