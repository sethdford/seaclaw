#include "test_framework.h"
#include "human/persona/humor.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Context evaluation ──────────────────────────────────────────── */

static void humor_fw_serious_context_low_appropriateness(void) {
    hu_humor_evaluation_t eval = {0};
    const char *conv = "My grandmother died last week. I'm really struggling.";
    HU_ASSERT(hu_humor_fw_evaluate_context(conv, strlen(conv), NULL, &eval) == HU_OK);
    HU_ASSERT(eval.appropriateness < 0.3f);
    HU_ASSERT(!eval.should_attempt);
}

static void humor_fw_light_context_high_appropriateness(void) {
    hu_humor_evaluation_t eval = {0};
    const char *conv = "Haha that's so funny! Tell me another joke lol";
    HU_ASSERT(hu_humor_fw_evaluate_context(conv, strlen(conv), NULL, &eval) == HU_OK);
    HU_ASSERT(eval.appropriateness > 0.7f);
    HU_ASSERT(eval.should_attempt);
}

static void humor_fw_neutral_context_moderate(void) {
    hu_humor_evaluation_t eval = {0};
    const char *conv = "What's the weather like tomorrow? I'm planning a trip.";
    HU_ASSERT(hu_humor_fw_evaluate_context(conv, strlen(conv), NULL, &eval) == HU_OK);
    HU_ASSERT(eval.appropriateness >= 0.3f && eval.appropriateness <= 0.7f);
}

static void humor_fw_serious_flag_suppresses(void) {
    hu_humor_evaluation_t eval = {0};
    hu_humor_context_t ctx = {.in_serious_context = true};
    const char *conv = "Just checking in, how are you?";
    HU_ASSERT(hu_humor_fw_evaluate_context(conv, strlen(conv), &ctx, &eval) == HU_OK);
    HU_ASSERT(eval.appropriateness < 0.3f);
}

static void humor_fw_with_persona_styles(void) {
    hu_humor_evaluation_t eval = {0};
    hu_humor_context_t ctx = {0};
    ctx.preferred_styles[0] = HU_HUMOR_FW_DRY;
    ctx.preferred_count = 1;
    ctx.contact_id = "alice";
    ctx.contact_id_len = 5;
    const char *conv = "That's hilarious, tell me more!";
    HU_ASSERT(hu_humor_fw_evaluate_context(conv, strlen(conv), &ctx, &eval) == HU_OK);
    HU_ASSERT(eval.persona_fit > 0.5f);
    HU_ASSERT(eval.audience_fit > 0.5f);
}

static void humor_fw_null_fails(void) {
    hu_humor_evaluation_t eval = {0};
    HU_ASSERT(hu_humor_fw_evaluate_context(NULL, 0, NULL, &eval) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Directive building ──────────────────────────────────────────── */

static void fw_directive_appropriate_returns_guidance(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_humor_evaluation_t eval = {
        .should_attempt = true,
        .appropriateness = 0.8f,
        .suggested_theory = HU_HUMOR_INCONGRUITY,
    };
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_humor_fw_build_directive(&alloc, &eval, NULL, &out, &out_len) == HU_OK);
    HU_ASSERT(out != NULL);
    HU_ASSERT(strstr(out, "HUMOR OPPORTUNITY") != NULL);
    HU_ASSERT(strstr(out, "incongruity") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void fw_directive_not_appropriate_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_humor_evaluation_t eval = {.should_attempt = false};
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_humor_fw_build_directive(&alloc, &eval, NULL, &out, &out_len) == HU_OK);
    HU_ASSERT(out == NULL);
}

static void fw_directive_null_fails(void) {
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_humor_fw_build_directive(NULL, NULL, NULL, &out, &out_len) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Response scoring ────────────────────────────────────────────── */

static void fw_score_humorous_response(void) {
    float score = 0.0f;
    const char *resp = "Haha well I'm terrible at cooking but at least my smoke detector works overtime!";
    HU_ASSERT(hu_humor_fw_score_response(resp, strlen(resp), NULL, &score) == HU_OK);
    HU_ASSERT(score > 0.2f);
}

static void fw_score_dry_response(void) {
    float score = 0.0f;
    const char *resp = "OK.";
    HU_ASSERT(hu_humor_fw_score_response(resp, strlen(resp), NULL, &score) == HU_OK);
    HU_ASSERT(score < 0.3f);
}

static void fw_score_null_fails(void) {
    float score = 0.0f;
    HU_ASSERT(hu_humor_fw_score_response(NULL, 0, NULL, &score) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Runner ──────────────────────────────────────────────────────── */

void run_humor_fw_tests(void) {
    HU_TEST_SUITE("Humor Framework");

    HU_RUN_TEST(humor_fw_serious_context_low_appropriateness);
    HU_RUN_TEST(humor_fw_light_context_high_appropriateness);
    HU_RUN_TEST(humor_fw_neutral_context_moderate);
    HU_RUN_TEST(humor_fw_serious_flag_suppresses);
    HU_RUN_TEST(humor_fw_with_persona_styles);
    HU_RUN_TEST(humor_fw_null_fails);

    HU_RUN_TEST(fw_directive_appropriate_returns_guidance);
    HU_RUN_TEST(fw_directive_not_appropriate_returns_null);
    HU_RUN_TEST(fw_directive_null_fails);

    HU_RUN_TEST(fw_score_humorous_response);
    HU_RUN_TEST(fw_score_dry_response);
    HU_RUN_TEST(fw_score_null_fails);
}
