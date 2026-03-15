#include "test_framework.h"
#include "human/eval.h"
#include <string.h>

static void test_eval_load(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json = "{\"name\":\"test-suite\",\"tasks\":[]}";
    hu_eval_suite_t suite;
    hu_error_t err = hu_eval_suite_load_json(&alloc, json, strlen(json), &suite);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(suite.name != NULL);
    HU_ASSERT_STR_EQ(suite.name, "test-suite");
    HU_ASSERT_EQ(suite.tasks_count, 0u);
    hu_eval_suite_free(&alloc, &suite);
}

static void test_eval_load_tasks(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *json = "{\"name\":\"reasoning\",\"tasks\":["
        "{\"id\":\"t1\",\"prompt\":\"What is 2+2?\",\"expected\":\"4\",\"category\":\"math\",\"difficulty\":1,\"timeout_ms\":5000},"
        "{\"id\":\"t2\",\"prompt\":\"Capital of France?\",\"expected\":\"Paris\",\"category\":\"knowledge\",\"difficulty\":1}"
        "]}";
    hu_eval_suite_t suite;
    hu_error_t err = hu_eval_suite_load_json(&alloc, json, strlen(json), &suite);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(suite.name, "reasoning");
    HU_ASSERT_EQ(suite.tasks_count, 2u);
    HU_ASSERT(suite.tasks != NULL);
    HU_ASSERT_STR_EQ(suite.tasks[0].id, "t1");
    HU_ASSERT_STR_EQ(suite.tasks[0].prompt, "What is 2+2?");
    HU_ASSERT_STR_EQ(suite.tasks[0].expected, "4");
    HU_ASSERT_STR_EQ(suite.tasks[0].category, "math");
    HU_ASSERT_EQ(suite.tasks[0].difficulty, 1);
    HU_ASSERT_EQ(suite.tasks[0].timeout_ms, 5000);
    HU_ASSERT_STR_EQ(suite.tasks[1].id, "t2");
    HU_ASSERT_STR_EQ(suite.tasks[1].prompt, "Capital of France?");
    HU_ASSERT_STR_EQ(suite.tasks[1].expected, "Paris");
    HU_ASSERT_STR_EQ(suite.tasks[1].category, "knowledge");
    HU_ASSERT_EQ(suite.tasks[1].difficulty, 1);
    HU_ASSERT_EQ(suite.tasks[1].timeout_ms, 5000);
    hu_eval_suite_free(&alloc, &suite);
}

static void test_eval_llm_judge_placeholder(void) {
    hu_allocator_t alloc = hu_system_allocator();
    bool passed = false;
    HU_ASSERT_EQ(hu_eval_check(&alloc, "The answer is 4", 15, "4", 1, HU_EVAL_LLM_JUDGE, &passed), HU_OK);
    HU_ASSERT(passed);
    passed = false;
    HU_ASSERT_EQ(hu_eval_check(&alloc, "Paris is the capital", 19, "Paris", 5, HU_EVAL_LLM_JUDGE, &passed), HU_OK);
    HU_ASSERT(passed);
    passed = true;
    HU_ASSERT_EQ(hu_eval_check(&alloc, "wrong answer", 12, "Paris", 5, HU_EVAL_LLM_JUDGE, &passed), HU_OK);
    HU_ASSERT(!passed);
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

static void test_eval_run_free(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_run_t run = {0};
    run.suite_name = alloc.alloc(alloc.ctx, 8);
    if (run.suite_name) memcpy(run.suite_name, "suite", 6);
    run.provider = alloc.alloc(alloc.ctx, 6);
    if (run.provider) memcpy(run.provider, "p", 2);
    run.model = alloc.alloc(alloc.ctx, 6);
    if (run.model) memcpy(run.model, "m", 2);
    run.results = alloc.alloc(alloc.ctx, sizeof(hu_eval_result_t));
    run.results_count = 1;
    if (run.results) {
        memset(&run.results[0], 0, sizeof(run.results[0]));
        run.results[0].task_id = alloc.alloc(alloc.ctx, 4);
        if (run.results[0].task_id) memcpy(run.results[0].task_id, "t1", 3);
        run.results[0].actual_output = alloc.alloc(alloc.ctx, 6);
        if (run.results[0].actual_output) { memcpy(run.results[0].actual_output, "out", 4); run.results[0].actual_output_len = 3; }
    }
    hu_eval_run_free(&alloc, &run);
    HU_ASSERT(run.suite_name == NULL);
    HU_ASSERT(run.results == NULL);
}

void run_eval_tests(void) {
    HU_TEST_SUITE("Evaluation Harness");
    HU_RUN_TEST(test_eval_load);
    HU_RUN_TEST(test_eval_load_tasks);
    HU_RUN_TEST(test_eval_exact);
    HU_RUN_TEST(test_eval_contains);
    HU_RUN_TEST(test_eval_numeric);
    HU_RUN_TEST(test_eval_llm_judge_placeholder);
    HU_RUN_TEST(test_eval_report);
    HU_RUN_TEST(test_eval_compare);
    HU_RUN_TEST(test_eval_run_free);
}
