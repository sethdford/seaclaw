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

void run_intelligence_tests(void) {
    HU_RUN_TEST(protective_topic_is_blocked_returns_true);
    HU_RUN_TEST(protective_topic_not_blocked_returns_false);
    HU_RUN_TEST(humor_select_serious_topic_returns_none);
}
