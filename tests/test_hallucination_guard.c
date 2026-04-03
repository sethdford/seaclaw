#include "test_framework.h"
#include "human/memory/hallucination_guard.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Claim extraction ────────────────────────────────────────────── */

static void extract_no_claims_from_plain_text(void) {
    hu_hallucination_result_t result = {0};
    const char *text = "The weather is nice today. Let me know if you need anything.";
    HU_ASSERT(hu_hallucination_extract_claims(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.claim_count == 0);
    HU_ASSERT(!result.needs_rewrite);
}

static void extract_single_remember_claim(void) {
    hu_hallucination_result_t result = {0};
    const char *text = "I remember when you told me about your trip to Paris.";
    HU_ASSERT(hu_hallucination_extract_claims(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.claim_count == 1);
    HU_ASSERT(result.unverified_count == 1);
    HU_ASSERT(result.needs_rewrite);
}

static void extract_multiple_claims(void) {
    hu_hallucination_result_t result = {0};
    const char *text = "You told me you like hiking. You mentioned your dog's name is Max.";
    HU_ASSERT(hu_hallucination_extract_claims(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.claim_count == 2);
}

static void extract_case_insensitive(void) {
    hu_hallucination_result_t result = {0};
    const char *text = "You Told Me about your project last week.";
    HU_ASSERT(hu_hallucination_extract_claims(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.claim_count == 1);
}

static void extract_null_args_fail(void) {
    hu_hallucination_result_t result = {0};
    HU_ASSERT(hu_hallucination_extract_claims(NULL, 0, &result) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_hallucination_extract_claims("hi", 2, NULL) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Verification ────────────────────────────────────────────────── */

static void verify_null_memory_noop(void) {
    hu_hallucination_result_t result = {0};
    result.claim_count = 1;
    result.unverified_count = 1;
    result.claims[0].status = HU_CLAIM_UNVERIFIED;
    HU_ASSERT(hu_hallucination_verify_claims(&result, NULL, NULL) == HU_OK);
}

static void verify_null_result_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT(hu_hallucination_verify_claims(NULL, NULL, &alloc) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Rewrite ─────────────────────────────────────────────────────── */

static void rewrite_no_claims_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hallucination_result_t result = {0};
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_hallucination_rewrite(&alloc, "hello", 5, &result, &out, &out_len) == HU_OK);
    HU_ASSERT(out == NULL);
}

static void rewrite_unverified_claim_hedges(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hallucination_result_t result = {0};
    const char *text = "I remember when you told me about hiking.";
    HU_ASSERT(hu_hallucination_extract_claims(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.claim_count > 0);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_hallucination_rewrite(&alloc, text, strlen(text), &result, &out, &out_len) == HU_OK);
    HU_ASSERT(out != NULL);
    HU_ASSERT(strstr(out, "might have mentioned") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void rewrite_null_args_fail(void) {
    hu_hallucination_result_t result = {0};
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_hallucination_rewrite(NULL, "x", 1, &result, &out, &out_len) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Full pipeline ───────────────────────────────────────────────── */

static void guard_no_claims_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    const char *text = "The weather is nice today.";
    HU_ASSERT(hu_hallucination_guard(&alloc, text, strlen(text), NULL, &out, &out_len) == HU_OK);
    HU_ASSERT(out == NULL);
}

static void guard_with_claims_rewrites(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    const char *text = "I remember when you told me you love pizza. That's great!";
    HU_ASSERT(hu_hallucination_guard(&alloc, text, strlen(text), NULL, &out, &out_len) == HU_OK);
    HU_ASSERT(out != NULL);
    HU_ASSERT(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void guard_null_args_fail(void) {
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT(hu_hallucination_guard(NULL, "x", 1, NULL, &out, &out_len) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Runner ──────────────────────────────────────────────────────── */

void run_hallucination_guard_tests(void) {
    HU_TEST_SUITE("Hallucination Guard");

    HU_RUN_TEST(extract_no_claims_from_plain_text);
    HU_RUN_TEST(extract_single_remember_claim);
    HU_RUN_TEST(extract_multiple_claims);
    HU_RUN_TEST(extract_case_insensitive);
    HU_RUN_TEST(extract_null_args_fail);

    HU_RUN_TEST(verify_null_memory_noop);
    HU_RUN_TEST(verify_null_result_fails);

    HU_RUN_TEST(rewrite_no_claims_returns_null);
    HU_RUN_TEST(rewrite_unverified_claim_hedges);
    HU_RUN_TEST(rewrite_null_args_fail);

    HU_RUN_TEST(guard_no_claims_returns_null);
    HU_RUN_TEST(guard_with_claims_rewrites);
    HU_RUN_TEST(guard_null_args_fail);
}
