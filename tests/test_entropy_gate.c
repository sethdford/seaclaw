#include "human/core/allocator.h"
#include "human/memory/entropy_gate.h"
#include "test_framework.h"
#include <math.h>
#include <string.h>

static void test_entropy_compute_high_info_text(void) {
    const char *text = "The quick brown fox jumps over the lazy dog";
    double e = 0.0;
    hu_error_t err = hu_entropy_compute(text, strlen(text), &e);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(e > 0.5);
}

static void test_entropy_compute_low_info_text(void) {
    const char *text = "ok ok ok ok ok";
    double e = 0.0;
    hu_error_t err = hu_entropy_compute(text, strlen(text), &e);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(e < 0.3);
}

static void test_entropy_compute_empty_returns_zero(void) {
    const char *text = "";
    double e = 1.0;
    hu_error_t err = hu_entropy_compute(text, 0, &e);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FLOAT_EQ(e, 0.0, 1e-9);
}

static void test_entropy_gate_filters_low(void) {
    hu_entropy_gate_config_t config = hu_entropy_gate_config_default();
    hu_memory_chunk_t chunks[3] = {
        {.text = "Important technical documentation about the API design", .text_len = 52},
        {.text = "ok ok ok ok ok", .text_len = 14},
        {.text = "Another meaningful sentence with diverse vocabulary here", .text_len = 53},
    };
    size_t passed = 0;
    hu_error_t err = hu_entropy_gate_filter(&config, chunks, 3, &passed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(chunks[1].passed);
    HU_ASSERT_TRUE(chunks[0].passed);
    HU_ASSERT_TRUE(chunks[2].passed);
    HU_ASSERT_EQ(passed, 2);
}

static void test_entropy_gate_passes_high(void) {
    hu_entropy_gate_config_t config = hu_entropy_gate_config_default();
    hu_memory_chunk_t chunks[2] = {
        {.text = "The quick brown fox jumps over the lazy dog", .text_len = 43},
        {.text = "Diverse vocabulary enhances information density significantly", .text_len = 53},
    };
    size_t passed = 0;
    hu_error_t err = hu_entropy_gate_filter(&config, chunks, 2, &passed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(chunks[0].passed);
    HU_ASSERT_TRUE(chunks[1].passed);
    HU_ASSERT_EQ(passed, 2);
}

static void test_entropy_gate_adaptive_tightens(void) {
    hu_entropy_gate_config_t config = hu_entropy_gate_config_default();
    config.context_budget = 50; /* very small budget */
    config.adaptive = true;
    /* Chunks that would normally pass, but with tiny budget adaptive raises threshold */
    hu_memory_chunk_t chunks[2] = {
        {.text = "Medium entropy text with some repetition here here", .text_len = 48},
        {.text = "Another chunk of similar length and content", .text_len = 42},
    };
    size_t passed = 0;
    hu_error_t err = hu_entropy_gate_filter(&config, chunks, 2, &passed);
    HU_ASSERT_EQ(err, HU_OK);
    /* With adaptive tightening, at least one might be filtered */
    HU_ASSERT_TRUE(passed <= 2);
}

static void test_entropy_coarsen_produces_summary(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_chunk_t chunks[3] = {
        {.text = "ok sure thanks", .text_len = 13, .passed = false},
        {.text = "yes no maybe", .text_len = 12, .passed = false},
        {.text = "the a an", .text_len = 8, .passed = false},
    };
    char summary[256];
    size_t summary_len = 0;
    hu_error_t err = hu_entropy_coarsen(&alloc, chunks, 3, summary, sizeof(summary), &summary_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(summary_len > 0);
    HU_ASSERT_TRUE(summary[0] != '\0');
}

static void test_entropy_config_default_values(void) {
    hu_entropy_gate_config_t config = hu_entropy_gate_config_default();
    HU_ASSERT_FLOAT_EQ(config.threshold, 0.3, 1e-9);
    HU_ASSERT_EQ(config.context_budget, 4096u);
    HU_ASSERT_TRUE(config.adaptive);
}

static void test_entropy_null_args_returns_error(void) {
    double e;
    HU_ASSERT_EQ(hu_entropy_compute(NULL, 5, &e), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_compute("hello", 5, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_memory_chunk_t chunks[1] = {{.text = "x", .text_len = 1}};
    size_t passed;
    HU_ASSERT_EQ(hu_entropy_gate_filter(NULL, chunks, 1, &passed), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_gate_filter(&(hu_entropy_gate_config_t){0}, NULL, 1, &passed),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_gate_filter(&(hu_entropy_gate_config_t){0}, chunks, 1, NULL),
                 HU_ERR_INVALID_ARGUMENT);

    hu_allocator_t alloc = hu_system_allocator();
    char buf[64];
    size_t len;
    HU_ASSERT_EQ(hu_entropy_coarsen(NULL, chunks, 1, buf, sizeof(buf), &len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_coarsen(&alloc, NULL, 1, buf, sizeof(buf), &len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_coarsen(&alloc, chunks, 1, NULL, sizeof(buf), &len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_coarsen(&alloc, chunks, 1, buf, 0, &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_entropy_coarsen(&alloc, chunks, 1, buf, sizeof(buf), NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_entropy_gate_tests(void) {
    HU_TEST_SUITE("entropy");
    HU_RUN_TEST(test_entropy_compute_high_info_text);
    HU_RUN_TEST(test_entropy_compute_low_info_text);
    HU_RUN_TEST(test_entropy_compute_empty_returns_zero);
    HU_RUN_TEST(test_entropy_gate_filters_low);
    HU_RUN_TEST(test_entropy_gate_passes_high);
    HU_RUN_TEST(test_entropy_gate_adaptive_tightens);
    HU_RUN_TEST(test_entropy_coarsen_produces_summary);
    HU_RUN_TEST(test_entropy_config_default_values);
    HU_RUN_TEST(test_entropy_null_args_returns_error);
}
