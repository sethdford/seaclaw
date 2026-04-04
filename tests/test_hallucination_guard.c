#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include "human/memory/hallucination_guard.h"
#include "test_framework.h"
#include <string.h>

static void hallucination_extract_finds_claims_in_factual_text(void) {
    const char *resp = "You told me you were planning a quiet weekend at home.";
    hu_hallucination_result_t result;
    HU_ASSERT_EQ(hu_hallucination_extract_claims(resp, strlen(resp), &result), HU_OK);
    HU_ASSERT_GT((long)result.claim_count, 0L);
}

static void hallucination_extract_empty_text_zero_claims(void) {
    hu_hallucination_result_t result;
    HU_ASSERT_EQ(hu_hallucination_extract_claims("", 0, &result), HU_OK);
    HU_ASSERT_EQ((long)result.claim_count, 0L);
}

static void hallucination_extract_null_returns_error(void) {
    hu_hallucination_result_t result;
    HU_ASSERT_EQ(hu_hallucination_extract_claims(NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void hallucination_rewrite_with_no_claims_returns_original(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *resp = "No memory claims in this sentence.";
    hu_hallucination_result_t result;
    memset(&result, 0, sizeof(result));
    result.needs_rewrite = false;

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_hallucination_rewrite(&alloc, resp, strlen(resp), &result, &out, &out_len),
                 HU_OK);
    /* No rewrite path: caller keeps the original response. */
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ((long)out_len, 0L);
}

static void hallucination_guard_null_memory_still_extracts(void) {
    const char *resp = "You mentioned you prefer morning walks.";
    hu_hallucination_result_t r;
    HU_ASSERT_EQ(hu_hallucination_extract_claims(resp, strlen(resp), &r), HU_OK);
    HU_ASSERT_GT((long)r.claim_count, 0L);

    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_hallucination_guard(&alloc, resp, strlen(resp), NULL, &out, &out_len), HU_OK);
    HU_ASSERT_NULL(out);
}

static void hallucination_guard_produces_output(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_none_memory_create(&alloc);
    HU_ASSERT_NOT_NULL(mem.vtable);

    const char *resp = "You told me you enjoy hiking on weekends.";
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_hallucination_guard(&alloc, resp, strlen(resp), &mem, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_GT((long)out_len, 0L);

    alloc.free(alloc.ctx, out, out_len + 1);
    mem.vtable->deinit(mem.ctx);
}

void run_hallucination_guard_tests(void) {
    HU_TEST_SUITE("hallucination_guard");
    HU_RUN_TEST(hallucination_extract_finds_claims_in_factual_text);
    HU_RUN_TEST(hallucination_extract_empty_text_zero_claims);
    HU_RUN_TEST(hallucination_extract_null_returns_error);
    HU_RUN_TEST(hallucination_rewrite_with_no_claims_returns_original);
    HU_RUN_TEST(hallucination_guard_null_memory_still_extracts);
    HU_RUN_TEST(hallucination_guard_produces_output);
}
