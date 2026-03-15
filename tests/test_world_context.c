#ifdef HU_ENABLE_SQLITE

#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/intelligence/world_model.h"
#include <string.h>

static void context_empty_text_returns_zero_entities(void) {
    hu_world_context_t ctx = {0};
    HU_ASSERT_EQ(hu_world_context_from_text("", 0, &ctx), HU_OK);
    HU_ASSERT_EQ(ctx.entity_count, 0u);
    HU_ASSERT_EQ(ctx.user_state_len, 0u);
}

static void context_extracts_capitalized_words(void) {
    hu_world_context_t ctx = {0};
    const char *text = "Alice went to Paris";
    HU_ASSERT_EQ(hu_world_context_from_text(text, 19, &ctx), HU_OK);
    HU_ASSERT_TRUE(ctx.entity_count >= 2u);
    HU_ASSERT_STR_EQ(ctx.entities[0], "Alice");
    HU_ASSERT_STR_EQ(ctx.entities[1], "Paris");
    HU_ASSERT_TRUE(ctx.user_state_len > 0);
}

static void context_limits_to_8_entities(void) {
    hu_world_context_t ctx = {0};
    /* 10 capitalized words - only first 8 should be stored */
    const char *text = "Alice Bob Charlie Dave Eve Frank Grace Henry Ian John";
    HU_ASSERT_EQ(hu_world_context_from_text(text, 51, &ctx), HU_OK);
    HU_ASSERT_EQ(ctx.entity_count, 8u);
    HU_ASSERT_STR_EQ(ctx.entities[0], "Alice");
    HU_ASSERT_STR_EQ(ctx.entities[7], "Henry");
}

static void context_sets_time_window(void) {
    hu_world_context_t ctx = {0};
    HU_ASSERT_EQ(hu_world_context_from_text("test", 4, &ctx), HU_OK);
    HU_ASSERT_TRUE(ctx.time_window_start < ctx.time_window_end);
    HU_ASSERT_TRUE(ctx.time_window_end - ctx.time_window_start >= 7200);
}

void run_world_context_tests(void) {
    HU_TEST_SUITE("world_context");
    HU_RUN_TEST(context_empty_text_returns_zero_entities);
    HU_RUN_TEST(context_extracts_capitalized_words);
    HU_RUN_TEST(context_limits_to_8_entities);
    HU_RUN_TEST(context_sets_time_window);
}

#else

void run_world_context_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
