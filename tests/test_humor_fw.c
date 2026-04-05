#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/persona/humor.h"
#include "test_framework.h"
#include <string.h>

/* API uses should_attempt / appropriateness; in_serious_context acts as serious/crisis gate. */

static void humor_evaluate_happy_context_allows_humor(void) {
    const char *conv = "That sketch was hilarious haha";
    size_t conv_len = strlen(conv);
    hu_humor_evaluation_t eval;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, conv_len, NULL, &eval), HU_OK);
    HU_ASSERT_TRUE(eval.appropriateness >= 0.85f);
    HU_ASSERT_TRUE(eval.should_attempt);
}

static void humor_evaluate_crisis_suppresses_humor(void) {
    const char *conv = "Thanks for checking in today.";
    size_t conv_len = strlen(conv);
    hu_humor_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.in_serious_context = true;
    hu_humor_evaluation_t eval;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, conv_len, &ctx, &eval), HU_OK);
    HU_ASSERT_TRUE(!eval.should_attempt || eval.appropriateness <= 0.35f);
}

static void humor_evaluate_null_conversation_returns_error(void) {
    hu_humor_evaluation_t eval;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(NULL, 0, NULL, &eval), HU_ERR_INVALID_ARGUMENT);
}

static void humor_build_directive_produces_string(void) {
    const char *conv = "Good one lol that really landed";
    hu_humor_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.preferred_styles[0] = HU_HUMOR_FW_OBSERVATIONAL;
    ctx.preferred_count = 1;
    hu_humor_evaluation_t eval;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, strlen(conv), &ctx, &eval), HU_OK);
    HU_ASSERT_TRUE(eval.should_attempt);

    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_humor_fw_build_directive(&alloc, &eval, &ctx, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_GT((long)out_len, 0L);
    HU_ASSERT_STR_CONTAINS(out, "HUMOR");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void humor_score_response_witty_gets_positive(void) {
    const char *resp =
        "Haha, I can see why you'd say that — it is a bit absurd when you "
        "step back and look at the whole picture.";
    hu_humor_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    float score = 0.0f;
    HU_ASSERT_EQ(hu_humor_fw_score_response(resp, strlen(resp), &ctx, &score), HU_OK);
    HU_ASSERT_TRUE(score > 0.0f);
}

static void humor_score_null_response_returns_error(void) {
    float score = 0.0f;
    HU_ASSERT_EQ(hu_humor_fw_score_response(NULL, 0, NULL, &score), HU_ERR_INVALID_ARGUMENT);
}

static void humor_persona_fit_preferred_style_boosts(void) {
    const char *conv = "haha that was great, tell me another one";
    hu_humor_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.preferred_styles[0] = HU_HUMOR_FW_OBSERVATIONAL;
    ctx.preferred_count = 1;
    ctx.risk_tolerance = 0.6f;
    hu_humor_evaluation_t with_pref;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, strlen(conv), &ctx, &with_pref), HU_OK);

    hu_humor_context_t ctx2;
    memset(&ctx2, 0, sizeof(ctx2));
    ctx2.risk_tolerance = 0.6f;
    hu_humor_evaluation_t no_pref;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, strlen(conv), &ctx2, &no_pref), HU_OK);

    HU_ASSERT_TRUE(with_pref.persona_fit > no_pref.persona_fit);
}

static void humor_audience_fit_uses_risk_tolerance(void) {
    const char *conv = "let me tell you something funny";
    hu_humor_context_t low_risk;
    memset(&low_risk, 0, sizeof(low_risk));
    low_risk.risk_tolerance = 0.1f;
    low_risk.contact_id = "alice";
    low_risk.contact_id_len = 5;

    hu_humor_context_t high_risk;
    memset(&high_risk, 0, sizeof(high_risk));
    high_risk.risk_tolerance = 0.9f;
    high_risk.contact_id = "alice";
    high_risk.contact_id_len = 5;

    hu_humor_evaluation_t lo, hi;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, strlen(conv), &low_risk, &lo), HU_OK);
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(conv, strlen(conv), &high_risk, &hi), HU_OK);
    HU_ASSERT_TRUE(hi.audience_fit >= lo.audience_fit);
}

static void humor_novelty_scales_with_conversation_length(void) {
    const char *short_conv = "hi";
    const char *long_conv =
        "So I was thinking about the whole situation and realized it's actually "
        "quite absurd when you consider all the different angles we could look at "
        "this from, and then we talked about it more and more.";
    hu_humor_evaluation_t short_eval, long_eval;
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(short_conv, strlen(short_conv), NULL, &short_eval), HU_OK);
    HU_ASSERT_EQ(hu_humor_fw_evaluate_context(long_conv, strlen(long_conv), NULL, &long_eval), HU_OK);
    HU_ASSERT_TRUE(long_eval.novelty > short_eval.novelty);
}

void run_humor_fw_tests(void) {
    HU_TEST_SUITE("humor_fw");
    HU_RUN_TEST(humor_evaluate_happy_context_allows_humor);
    HU_RUN_TEST(humor_evaluate_crisis_suppresses_humor);
    HU_RUN_TEST(humor_evaluate_null_conversation_returns_error);
    HU_RUN_TEST(humor_build_directive_produces_string);
    HU_RUN_TEST(humor_score_response_witty_gets_positive);
    HU_RUN_TEST(humor_score_null_response_returns_error);
    HU_RUN_TEST(humor_persona_fit_preferred_style_boosts);
    HU_RUN_TEST(humor_audience_fit_uses_risk_tolerance);
    HU_RUN_TEST(humor_novelty_scales_with_conversation_length);
}
