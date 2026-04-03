#include "human/context/behavioral.h"
#include "human/core/allocator.h"
#include "human/persona.h"
#include "test_framework.h"
#include <string.h>

static void double_text_default_config_low_closeness(void) {
    hu_double_text_config_t config = {
        .probability = 0.15,
        .min_gap_seconds = 30,
        .max_gap_seconds = 300,
        .only_close_friends = true,
    };
    bool result = hu_should_double_text(0.1, 1000, 2000, &config, 42);
    (void)result;
}

static void double_text_build_prompt_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_double_text_build_prompt(&alloc, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void double_text_high_closeness_may_trigger(void) {
    hu_double_text_config_t config = {
        .probability = 1.0,
        .min_gap_seconds = 1,
        .max_gap_seconds = 5,
        .only_close_friends = true,
    };
    bool result = hu_should_double_text(0.9, 100, 200, &config, 42);
    (void)result;
}

static void double_text_not_only_close_friends(void) {
    hu_double_text_config_t config = {
        .probability = 1.0,
        .min_gap_seconds = 1,
        .max_gap_seconds = 5,
        .only_close_friends = false,
    };
    bool result = hu_should_double_text(0.1, 100, 200, &config, 42);
    (void)result;
}

static void double_text_gap_too_small_returns_false(void) {
    hu_double_text_config_t config = {
        .probability = 1.0,
        .min_gap_seconds = 1000,
        .max_gap_seconds = 5000,
        .only_close_friends = false,
    };
    bool result = hu_should_double_text(0.9, 100, 101, &config, 42);
    HU_ASSERT_FALSE(result);
}

static void bookend_check_morning_close_friend(void) {
    hu_bookend_type_t bt = hu_bookend_check(8, true, false, 42);
    (void)bt;
}

static void bookend_check_already_sent_returns_none(void) {
    hu_bookend_type_t bt = hu_bookend_check(8, true, true, 42);
    HU_ASSERT_EQ(bt, HU_BOOKEND_NONE);
}

static void bookend_type_str_non_null(void) {
    HU_ASSERT_NOT_NULL(hu_bookend_type_str(HU_BOOKEND_NONE));
    HU_ASSERT_NOT_NULL(hu_bookend_type_str(HU_BOOKEND_MORNING));
    HU_ASSERT_NOT_NULL(hu_bookend_type_str(HU_BOOKEND_GOODNIGHT));
}

static void timezone_compute_valid_result(void) {
    hu_timezone_info_t tz = hu_timezone_compute(-5, 1700000000000ULL);
    HU_ASSERT_TRUE(tz.local_hour < 24);
}

/* --- F28: Mirroring identity boundary tests --- */

static void mirror_conflicts_abbreviations_with_avoided_vocab(void) {
    hu_mirror_analysis_t analysis = {.uses_abbreviations = true};
    hu_persona_t p = {0};
    char *avoided[] = {(char *)"u", (char *)"ur"};
    p.avoided_vocab = avoided;
    p.avoided_vocab_count = 2;
    HU_ASSERT_TRUE(hu_mirror_conflicts_with_identity(&analysis, (const struct hu_persona *)&p));
}

static void mirror_no_conflict_without_abbreviations(void) {
    hu_mirror_analysis_t analysis = {.uses_abbreviations = false, .uses_lowercase = true};
    hu_persona_t p = {0};
    char *avoided[] = {(char *)"u", (char *)"ur"};
    p.avoided_vocab = avoided;
    p.avoided_vocab_count = 2;
    HU_ASSERT_FALSE(hu_mirror_conflicts_with_identity(&analysis, (const struct hu_persona *)&p));
}

static void mirror_conflicts_abbreviations_with_invariant(void) {
    hu_mirror_analysis_t analysis = {.uses_abbreviations = true};
    hu_persona_t p = {0};
    char *invariants[] = {(char *)"Never uses abbreviations"};
    p.character_invariants = invariants;
    p.character_invariants_count = 1;
    HU_ASSERT_TRUE(hu_mirror_conflicts_with_identity(&analysis, (const struct hu_persona *)&p));
}

static void mirror_conflicts_lowercase_with_invariant(void) {
    hu_mirror_analysis_t analysis = {.uses_lowercase = true};
    hu_persona_t p = {0};
    char *invariants[] = {(char *)"Never types in lowercase"};
    p.character_invariants = invariants;
    p.character_invariants_count = 1;
    HU_ASSERT_TRUE(hu_mirror_conflicts_with_identity(&analysis, (const struct hu_persona *)&p));
}

static void mirror_no_conflict_null_persona(void) {
    hu_mirror_analysis_t analysis = {.uses_abbreviations = true};
    HU_ASSERT_FALSE(hu_mirror_conflicts_with_identity(&analysis, NULL));
}

static void mirror_no_conflict_null_analysis(void) {
    hu_persona_t p = {0};
    HU_ASSERT_FALSE(hu_mirror_conflicts_with_identity(NULL, (const struct hu_persona *)&p));
}

static void mirror_build_directive_persona_filters_abbreviations(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mirror_analysis_t analysis = {
        .uses_abbreviations = true,
        .uses_lowercase = false,
        .avg_msg_length = 50.0,
    };
    hu_persona_t p = {0};
    char *avoided[] = {(char *)"u", (char *)"rn"};
    p.avoided_vocab = avoided;
    p.avoided_vocab_count = 2;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mirror_build_directive_persona(
        &alloc, &analysis, (const struct hu_persona *)&p, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    /* The abbreviation sentence should be filtered out */
    if (out) {
        HU_ASSERT_TRUE(strstr(out, "abbreviation") == NULL);
        /* But message length directive should survive */
        HU_ASSERT_TRUE(strstr(out, "chars") != NULL);
        alloc.free(alloc.ctx, out, out_len + 1);
    }
}

static void mirror_build_directive_persona_null_persona_passes_through(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mirror_analysis_t analysis = {
        .uses_abbreviations = true,
        .avg_msg_length = 30.0,
    };

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mirror_build_directive_persona(&alloc, &analysis, NULL, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "abbreviation") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void mirror_identity_bounds_filters_correctly(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *directive = "[LINGUISTIC MIRROR]: They use abbreviations — ok to use 'u' and 'rn'. "
                            "Keep messages around 40 chars.";
    hu_persona_t p = {0};
    char *invariants[] = {(char *)"Never uses abbreviations"};
    p.character_invariants = invariants;
    p.character_invariants_count = 1;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mirror_check_identity_bounds(&alloc, directive, strlen(directive),
                                                     (const struct hu_persona *)&p, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    /* Abbreviation sentence should be removed */
    HU_ASSERT_TRUE(strstr(out, "abbreviation") == NULL);
    /* Keep messages sentence should remain */
    HU_ASSERT_TRUE(strstr(out, "chars") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

void run_behavioral_tests(void) {
    HU_TEST_SUITE("behavioral");
    HU_RUN_TEST(double_text_default_config_low_closeness);
    HU_RUN_TEST(double_text_build_prompt_succeeds);
    HU_RUN_TEST(double_text_high_closeness_may_trigger);
    HU_RUN_TEST(double_text_not_only_close_friends);
    HU_RUN_TEST(double_text_gap_too_small_returns_false);
    HU_RUN_TEST(bookend_check_morning_close_friend);
    HU_RUN_TEST(bookend_check_already_sent_returns_none);
    HU_RUN_TEST(bookend_type_str_non_null);
    HU_RUN_TEST(timezone_compute_valid_result);
    /* Mirroring identity boundary */
    HU_RUN_TEST(mirror_conflicts_abbreviations_with_avoided_vocab);
    HU_RUN_TEST(mirror_no_conflict_without_abbreviations);
    HU_RUN_TEST(mirror_conflicts_abbreviations_with_invariant);
    HU_RUN_TEST(mirror_conflicts_lowercase_with_invariant);
    HU_RUN_TEST(mirror_no_conflict_null_persona);
    HU_RUN_TEST(mirror_no_conflict_null_analysis);
    HU_RUN_TEST(mirror_build_directive_persona_filters_abbreviations);
    HU_RUN_TEST(mirror_build_directive_persona_null_persona_passes_through);
    HU_RUN_TEST(mirror_identity_bounds_filters_correctly);
}
