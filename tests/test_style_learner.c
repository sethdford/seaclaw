/* Tests for src/persona/style_learner.c */
#ifdef HU_ENABLE_PERSONA

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/persona.h"
#include "test_framework.h"
#include <string.h>

static void style_reanalyze_null_alloc_returns_invalid(void) {
    hu_error_t err = hu_persona_style_reanalyze(
        NULL, NULL, NULL, 0, NULL, "test", 4, NULL, 0, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void style_reanalyze_null_name_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_style_reanalyze(
        &alloc, NULL, NULL, 0, NULL, NULL, 4, NULL, 0, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void style_reanalyze_zero_name_len_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_style_reanalyze(
        &alloc, NULL, NULL, 0, NULL, "test", 0, NULL, 0, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void style_reanalyze_null_memory_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_style_reanalyze(
        &alloc, NULL, NULL, 0, NULL, "test", 4, NULL, 0, NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
}

static void style_reanalyze_with_channel_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_style_reanalyze(
        &alloc, NULL, "gpt-4", 5, NULL, "tester", 6, "discord", 7, NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
}

static void style_reanalyze_with_contact_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_style_reanalyze(
        &alloc, NULL, NULL, 0, NULL, "tester", 6, "cli", 3, "user_a", 6);
    HU_ASSERT_EQ(err, HU_OK);
}

void run_style_learner_tests(void) {
    HU_TEST_SUITE("StyleLearner");

    HU_RUN_TEST(style_reanalyze_null_alloc_returns_invalid);
    HU_RUN_TEST(style_reanalyze_null_name_returns_invalid);
    HU_RUN_TEST(style_reanalyze_zero_name_len_returns_invalid);
    HU_RUN_TEST(style_reanalyze_null_memory_returns_ok);
    HU_RUN_TEST(style_reanalyze_with_channel_returns_ok);
    HU_RUN_TEST(style_reanalyze_with_contact_returns_ok);
}

#else

void run_style_learner_tests(void) { (void)0; }

#endif /* HU_ENABLE_PERSONA */
