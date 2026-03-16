#include "test_framework.h"
#include "human/intelligence/weakness.h"
#include "human/eval.h"
/* Tests cover both weakness.c and weakness_analyzer.c */
#include <string.h>

static void test_weakness_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weakness_report_t report = {0};
    hu_eval_run_t run = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(NULL, &run, NULL, &report), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, NULL, NULL, &report), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_weakness_empty_run(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_run_t run = {0};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, NULL, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 0u);
    HU_ASSERT_NULL(report.items);
}

static void test_weakness_all_pass(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_result_t results[2] = {
        {.task_id = "t1", .passed = true},
        {.task_id = "t2", .passed = true},
    };
    hu_eval_run_t run = {.results = results, .results_count = 2, .passed = 2};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, NULL, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 0u);
    HU_ASSERT_NULL(report.items);
}

static void test_weakness_all_fail(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_result_t results[3] = {
        {.task_id = "t1", .passed = false, .actual_output = "wrong", .actual_output_len = 5},
        {.task_id = "t2", .passed = false, .actual_output = NULL, .actual_output_len = 0},
        {.task_id = "t3", .passed = false, .actual_output = "nope", .actual_output_len = 4},
    };
    hu_eval_run_t run = {.results = results, .results_count = 3, .failed = 3};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, NULL, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 3u);
    HU_ASSERT_NOT_NULL(report.items);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_category_math(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[1] = {{.id = "t1", .prompt = "2+2", .prompt_len = 3,
                                .expected = "4", .expected_len = 1, .category = "math"}};
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "5", .actual_output_len = 1},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, &suite, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 1u);
    HU_ASSERT_EQ(report.items[0].type, HU_WEAKNESS_REASONING);
    HU_ASSERT_EQ(report.by_type[HU_WEAKNESS_REASONING], 1u);
    HU_ASSERT_STR_EQ(report.items[0].category, "math");
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_category_knowledge(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[1] = {{.id = "t1", .prompt = "Capital?", .prompt_len = 8,
                                .expected = "Paris", .expected_len = 5, .category = "geography"}};
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "London", .actual_output_len = 6},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, &suite, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 1u);
    HU_ASSERT_EQ(report.items[0].type, HU_WEAKNESS_KNOWLEDGE);
    HU_ASSERT_EQ(report.by_type[HU_WEAKNESS_KNOWLEDGE], 1u);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_category_tool(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[1] = {{.id = "t1", .prompt = "call fn", .prompt_len = 7,
                                .expected = "ok", .expected_len = 2, .category = "tool_use"}};
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "err", .actual_output_len = 3},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, &suite, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 1u);
    HU_ASSERT_EQ(report.items[0].type, HU_WEAKNESS_TOOL_USE);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_category_format(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[1] = {{.id = "t1", .prompt = "fmt", .prompt_len = 3,
                                .expected = "json", .expected_len = 4, .category = "output_format"}};
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "text", .actual_output_len = 4},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, &suite, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 1u);
    HU_ASSERT_EQ(report.items[0].type, HU_WEAKNESS_FORMAT);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_output_fallback_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = NULL, .actual_output_len = 0},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, NULL, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 1u);
    HU_ASSERT_EQ(report.items[0].type, HU_WEAKNESS_TOOL_USE);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_mixed_pass_fail(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[3] = {
        {.id = "t1", .prompt = "q1", .prompt_len = 2, .expected = "a1", .expected_len = 2, .category = "math"},
        {.id = "t2", .prompt = "q2", .prompt_len = 2, .expected = "a2", .expected_len = 2, .category = "knowledge"},
        {.id = "t3", .prompt = "q3", .prompt_len = 2, .expected = "a3", .expected_len = 2, .category = "tool_use"},
    };
    hu_eval_suite_t suite = {.name = "mix", .tasks = tasks, .tasks_count = 3};
    hu_eval_result_t results[3] = {
        {.task_id = "t1", .passed = false, .actual_output = "x", .actual_output_len = 1},
        {.task_id = "t2", .passed = true},
        {.task_id = "t3", .passed = false, .actual_output = "y", .actual_output_len = 1},
    };
    hu_eval_run_t run = {.results = results, .results_count = 3, .passed = 1, .failed = 2};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, &suite, &report), HU_OK);
    HU_ASSERT_EQ(report.count, 2u);
    HU_ASSERT_EQ(report.by_type[HU_WEAKNESS_REASONING], 1u);
    HU_ASSERT_EQ(report.by_type[HU_WEAKNESS_TOOL_USE], 1u);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_type_str(void) {
    HU_ASSERT_STR_EQ(hu_weakness_type_str(HU_WEAKNESS_REASONING), "reasoning");
    HU_ASSERT_STR_EQ(hu_weakness_type_str(HU_WEAKNESS_KNOWLEDGE), "knowledge");
    HU_ASSERT_STR_EQ(hu_weakness_type_str(HU_WEAKNESS_TOOL_USE), "tool_use");
    HU_ASSERT_STR_EQ(hu_weakness_type_str(HU_WEAKNESS_FORMAT), "format");
    HU_ASSERT_STR_EQ(hu_weakness_type_str(HU_WEAKNESS_UNKNOWN), "unknown");
}

static void test_weakness_description_populated(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[1] = {{.id = "t1", .prompt = "q", .prompt_len = 1,
                                .expected = "42", .expected_len = 2, .category = "math"}};
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "43", .actual_output_len = 2},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1};
    hu_weakness_report_t report = {0};
    HU_ASSERT_EQ(hu_weakness_analyze(&alloc, &run, &suite, &report), HU_OK);
    HU_ASSERT_TRUE(report.items[0].description_len > 0);
    HU_ASSERT_TRUE(report.items[0].suggested_fix_len > 0);
    hu_weakness_report_free(&alloc, &report);
}

static void test_weakness_analyze_summary_empty_run(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_run_t run = {.results = NULL, .results_count = 0, .passed = 0, .failed = 0};
    hu_weakness_summary_t summaries[8] = {{0}};
    size_t out_count = 99;
    HU_ASSERT_EQ(hu_weakness_analyze_summary(&alloc, &run, NULL, summaries, 8, &out_count),
                 HU_OK);
    HU_ASSERT_EQ(out_count, 0u);
}

static void test_weakness_analyze_summary_groups_failures(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_task_t tasks[3] = {
        {.id = "t1", .prompt = "2+2", .prompt_len = 3, .expected = "4", .expected_len = 1,
         .category = "math"},
        {.id = "t2", .prompt = "cap", .prompt_len = 3, .expected = "Paris", .expected_len = 5,
         .category = "geography"},
        {.id = "t3", .prompt = "fmt", .prompt_len = 3, .expected = "json", .expected_len = 4,
         .category = "output_format"},
    };
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 3};
    hu_eval_result_t results[3] = {
        {.task_id = "t1", .passed = false, .actual_output = "5", .actual_output_len = 1},
        {.task_id = "t2", .passed = false, .actual_output = "London", .actual_output_len = 6},
        {.task_id = "t3", .passed = false, .actual_output = "text", .actual_output_len = 4},
    };
    hu_eval_run_t run = {.results = results, .results_count = 3, .failed = 3};
    hu_weakness_summary_t summaries[8] = {{0}};
    size_t out_count = 0;
    HU_ASSERT_EQ(hu_weakness_analyze_summary(&alloc, &run, &suite, summaries, 8, &out_count),
                 HU_OK);
    HU_ASSERT_TRUE(out_count >= 2u);
    HU_ASSERT_TRUE(out_count <= 3u);
}

static void test_weakness_free_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weakness_report_free(NULL, NULL);
    hu_weakness_report_free(&alloc, NULL);
    hu_weakness_report_t report = {0};
    hu_weakness_report_free(&alloc, &report);
}

void run_weakness_tests(void) {
    HU_TEST_SUITE("Weakness Analyzer");
    HU_RUN_TEST(test_weakness_null_args);
    HU_RUN_TEST(test_weakness_empty_run);
    HU_RUN_TEST(test_weakness_all_pass);
    HU_RUN_TEST(test_weakness_all_fail);
    HU_RUN_TEST(test_weakness_category_math);
    HU_RUN_TEST(test_weakness_category_knowledge);
    HU_RUN_TEST(test_weakness_category_tool);
    HU_RUN_TEST(test_weakness_category_format);
    HU_RUN_TEST(test_weakness_output_fallback_empty);
    HU_RUN_TEST(test_weakness_mixed_pass_fail);
    HU_RUN_TEST(test_weakness_type_str);
    HU_RUN_TEST(test_weakness_description_populated);
    HU_RUN_TEST(test_weakness_analyze_summary_empty_run);
    HU_RUN_TEST(test_weakness_analyze_summary_groups_failures);
    HU_RUN_TEST(test_weakness_free_null);
}
