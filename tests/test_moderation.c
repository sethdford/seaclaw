#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/security/moderation.h"
#include "human/persona/relationship.h"
#include <string.h>

static void test_mod_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "hello how are you", 17, &result), HU_OK);
    HU_ASSERT(!result.flagged);
}

static void test_mod_violence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "kill them with violence and murder", 34, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.violence);
}

static void test_mod_null(void) {
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(NULL, "text", 4, &result), HU_ERR_INVALID_ARGUMENT);
}

static void test_mod_empty_string(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    memset(&result, 0xff, sizeof(result));
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "", 0, &result), HU_OK);
    HU_ASSERT(!result.flagged);
}

static void test_mod_hate_speech(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "hate group rhetoric", 19, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.hate);
}

static void test_mod_self_harm(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "mentions suicide ideation", 25, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

static void test_mod_null_result(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "safe text", 9, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_mod_null_text(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void test_mod_zero_length(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    static const char buf[] = "violence";
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, buf, 0, &result), HU_OK);
    HU_ASSERT(!result.flagged);
}

/* SHIELD-005: Coded crisis language detection */
static void test_mod_coded_crisis_kms(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "i wanna kms", 11, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

static void test_mod_coded_crisis_unalive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "i want to unalive myself", 24, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

static void test_mod_coded_crisis_end_it_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "just want to end it all", 23, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

static void test_mod_coded_crisis_better_off_without_me(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "everyone's better off without me", 32, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

static void test_mod_coded_crisis_no_reason(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "no reason to go on anymore", 26, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

/* Spaced-letter evasion: norm collapses to "selfharm", raw phrase check misses */
static void test_mod_spaced_self_harm(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "s e l f h a r m", 15, &result), HU_OK);
    HU_ASSERT(result.self_harm);
    HU_ASSERT(result.flagged);
}

/* SHIELD-005: Crisis response builder */
static void test_crisis_response_build(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_crisis_response_build(&alloc, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(out_len > 0);
    /* Must contain 988 lifeline reference */
    HU_ASSERT_NOT_NULL(strstr(out, "988"));
    HU_ASSERT_NOT_NULL(strstr(out, "741741"));
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_crisis_response_build_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_crisis_response_build(NULL, &out, &out_len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_crisis_response_build(&alloc, NULL, &out_len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_crisis_response_build(&alloc, &out, NULL), HU_ERR_INVALID_ARGUMENT);
}

/* Graduated severity: venting phrase → low score, NOT flagged */
static void test_mod_venting_whats_the_point(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "ugh what's the point", 20, &result), HU_OK);
    HU_ASSERT(result.self_harm);
    HU_ASSERT(result.self_harm_score < 0.6);
    HU_ASSERT(!result.flagged);
}

static void test_mod_venting_cant_do_this(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "i can't do this anymore", 23, &result), HU_OK);
    HU_ASSERT(result.self_harm);
    HU_ASSERT(result.self_harm_score < 0.6);
    HU_ASSERT(!result.flagged);
}

/* High-specificity phrases remain flagged */
static void test_mod_crisis_kms_flagged(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "i want to kms", 13, &result), HU_OK);
    HU_ASSERT(result.self_harm);
    HU_ASSERT(result.self_harm_score >= 0.8);
    HU_ASSERT(result.flagged);
}

/* Relationship stage advances with turns */
static void test_relationship_advances_with_turns(void) {
    hu_relationship_state_t state;
    memset(&state, 0, sizeof(state));
    state.stage = HU_REL_NEW;
    hu_relationship_update(&state, 25);
    HU_ASSERT(state.stage >= HU_REL_FAMILIAR);
    hu_relationship_update(&state, 60);
    HU_ASSERT(state.stage >= HU_REL_TRUSTED);
    hu_relationship_update(&state, 120);
    HU_ASSERT(state.stage >= HU_REL_DEEP);
}

/* Word boundary: "skill" should NOT match "kill" */
static void test_mod_word_boundary_skill_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "improve your skill", 18, &result), HU_OK);
    HU_ASSERT(!result.violence);
    HU_ASSERT(!result.flagged);
}

/* Word boundary: "kill" alone IS flagged */
static void test_mod_word_boundary_kill_flagged(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "kill the process", 16, &result), HU_OK);
    HU_ASSERT(result.violence);
    HU_ASSERT(result.flagged);
}

/* Confusable normalization: "k1ll" normalizes to "kill" */
static void test_mod_confusable_k1ll(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "k1ll them", 9, &result), HU_OK);
    HU_ASSERT(result.violence);
    HU_ASSERT(result.flagged);
}

/* Confusable normalization: "k i l l" with spaces */
static void test_mod_confusable_spaced(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "k i l l them", 12, &result), HU_OK);
    HU_ASSERT(result.violence);
    HU_ASSERT(result.flagged);
}

/* Sexual category detection */
static void test_mod_sexual_nsfw(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "this is nsfw content", 20, &result), HU_OK);
    HU_ASSERT(result.sexual);
    HU_ASSERT(result.flagged);
}

/* Sexual: clean text stays clean */
static void test_mod_sexual_clean(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "a normal conversation", 21, &result), HU_OK);
    HU_ASSERT(!result.sexual);
}

void run_moderation_tests(void) {
    HU_TEST_SUITE("Content Moderation");
    HU_RUN_TEST(test_mod_safe);
    HU_RUN_TEST(test_mod_violence);

    HU_TEST_SUITE("Word Boundary Matching");
    HU_RUN_TEST(test_mod_word_boundary_skill_safe);
    HU_RUN_TEST(test_mod_word_boundary_kill_flagged);

    HU_TEST_SUITE("Confusable Normalization");
    HU_RUN_TEST(test_mod_confusable_k1ll);
    HU_RUN_TEST(test_mod_confusable_spaced);

    HU_TEST_SUITE("Sexual Content Detection");
    HU_RUN_TEST(test_mod_sexual_nsfw);
    HU_RUN_TEST(test_mod_sexual_clean);
    HU_RUN_TEST(test_mod_null);
    HU_RUN_TEST(test_mod_empty_string);
    HU_RUN_TEST(test_mod_hate_speech);
    HU_RUN_TEST(test_mod_self_harm);
    HU_RUN_TEST(test_mod_null_result);
    HU_RUN_TEST(test_mod_null_text);
    HU_RUN_TEST(test_mod_zero_length);

    HU_TEST_SUITE("Crisis Detection (SHIELD-005)");
    HU_RUN_TEST(test_mod_coded_crisis_kms);
    HU_RUN_TEST(test_mod_coded_crisis_unalive);
    HU_RUN_TEST(test_mod_coded_crisis_end_it_all);
    HU_RUN_TEST(test_mod_coded_crisis_better_off_without_me);
    HU_RUN_TEST(test_mod_coded_crisis_no_reason);
    HU_RUN_TEST(test_mod_spaced_self_harm);

    HU_TEST_SUITE("Crisis Response Builder (SHIELD-005)");
    HU_RUN_TEST(test_crisis_response_build);
    HU_RUN_TEST(test_crisis_response_build_null_args);

    HU_TEST_SUITE("Graduated Severity (Venting vs Crisis)");
    HU_RUN_TEST(test_mod_venting_whats_the_point);
    HU_RUN_TEST(test_mod_venting_cant_do_this);
    HU_RUN_TEST(test_mod_crisis_kms_flagged);

    HU_TEST_SUITE("Relationship Stage Progression");
    HU_RUN_TEST(test_relationship_advances_with_turns);
}
