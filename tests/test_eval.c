#include "test_framework.h"
#include "human/eval.h"

static void test_eval_load(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json = "{\"name\":\"test-suite\",\"tasks\":[]}";
    hu_eval_suite_t suite;
    hu_error_t err = hu_eval_suite_load_json(&alloc, json, strlen(json), &suite);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(suite.name != NULL);
    HU_ASSERT_STR_EQ(suite.name, "test-suite");
    hu_eval_suite_free(&alloc, &suite);
}

static void test_eval_exact(void) {
    hu_allocator_t alloc = hu_system_allocator();
    bool passed = false;
    HU_ASSERT_EQ(hu_eval_check(&alloc, "hello", 5, "hello", 5, HU_EVAL_EXACT, &passed), HU_OK);
    HU_ASSERT(passed);
    HU_ASSERT_EQ(hu_eval_check(&alloc, "hello", 5, "world", 5, HU_EVAL_EXACT, &passed), HU_OK);
    HU_ASSERT(!passed);
}

static void test_eval_contains(void) {
    hu_allocator_t alloc = hu_system_allocator();
    bool passed = false;
    HU_ASSERT_EQ(hu_eval_check(&alloc, "hello world", 11, "world", 5, HU_EVAL_CONTAINS, &passed), HU_OK);
    HU_ASSERT(passed);
}

static void test_eval_numeric(void) {
    hu_allocator_t alloc = hu_system_allocator();
    bool passed = false;
    HU_ASSERT_EQ(hu_eval_check(&alloc, "3.14", 4, "3.14", 4, HU_EVAL_NUMERIC_CLOSE, &passed), HU_OK);
    HU_ASSERT(passed);
}

static void test_eval_report(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_run_t run = { .suite_name = "s", .provider = "p", .model = "m", .passed = 5, .failed = 1, .pass_rate = 83.33, .total_elapsed_ms = 1000 };
    char *out = NULL; size_t out_len = 0;
    HU_ASSERT_EQ(hu_eval_report_json(&alloc, &run, &out, &out_len), HU_OK);
    HU_ASSERT(out != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_eval_compare(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_run_t b = { .pass_rate = 80.0 };
    hu_eval_run_t c = { .pass_rate = 90.0 };
    char *report = NULL; size_t rlen = 0;
    HU_ASSERT_EQ(hu_eval_compare(&alloc, &b, &c, &report, &rlen), HU_OK);
    HU_ASSERT(report != NULL);
    alloc.free(alloc.ctx, report, rlen + 1);
}

void run_eval_tests(void) {
    HU_TEST_SUITE("Evaluation Harness");
    HU_RUN_TEST(test_eval_load);
    HU_RUN_TEST(test_eval_exact);
    HU_RUN_TEST(test_eval_contains);
    HU_RUN_TEST(test_eval_numeric);
    HU_RUN_TEST(test_eval_report);
    HU_RUN_TEST(test_eval_compare);
}
