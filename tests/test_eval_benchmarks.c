#include "test_framework.h"
#include "human/eval_benchmarks.h"
#include "human/eval_dashboard.h"
#include "human/eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *REASONING_JSON =
    "{\"name\":\"reasoning-basic\",\"tasks\":["
    "{\"id\":\"r1\",\"prompt\":\"What is 15 + 27?\",\"expected\":\"42\",\"category\":\"arithmetic\",\"difficulty\":1,\"timeout_ms\":5000}"
    "]}";

static void benchmark_load_gaia_format(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_suite_t suite;
    hu_error_t err = hu_benchmark_load(&alloc, HU_BENCHMARK_GAIA, REASONING_JSON, strlen(REASONING_JSON), &suite);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(suite.name != NULL);
    HU_ASSERT(strncmp(suite.name, "gaia-", 5) == 0);
    hu_eval_suite_free(&alloc, &suite);
}

static void benchmark_load_swe_bench_format(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_suite_t suite;
    hu_error_t err = hu_benchmark_load(&alloc, HU_BENCHMARK_SWE_BENCH, REASONING_JSON, strlen(REASONING_JSON), &suite);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(suite.name != NULL);
    HU_ASSERT(strncmp(suite.name, "swe-bench-", 10) == 0);
    hu_eval_suite_free(&alloc, &suite);
}

static void benchmark_load_tool_use_format(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_suite_t suite;
    hu_error_t err = hu_benchmark_load(&alloc, HU_BENCHMARK_TOOL_USE, REASONING_JSON, strlen(REASONING_JSON), &suite);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(suite.name != NULL);
    HU_ASSERT(strncmp(suite.name, "tool-use-", 9) == 0);
    hu_eval_suite_free(&alloc, &suite);
}

static void benchmark_type_name_returns_correct(void) {
    HU_ASSERT_STR_EQ(hu_benchmark_type_name(HU_BENCHMARK_GAIA), "gaia");
    HU_ASSERT_STR_EQ(hu_benchmark_type_name(HU_BENCHMARK_SWE_BENCH), "swe-bench");
    HU_ASSERT_STR_EQ(hu_benchmark_type_name(HU_BENCHMARK_TOOL_USE), "tool-use");
}

static void dashboard_render_single_run(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_run_t run = {0};
    run.suite_name = alloc.alloc(alloc.ctx, 20);
    if (run.suite_name) memcpy(run.suite_name, "reasoning-basic", 16);
    run.passed = 8;
    run.failed = 2;
    run.pass_rate = 80.0;
    run.total_elapsed_ms = 120;

    FILE *f = tmpfile();
    HU_ASSERT(f != NULL);
    hu_error_t err = hu_eval_dashboard_render(&alloc, f, &run, 1);
    HU_ASSERT_EQ(err, HU_OK);

    rewind(f);
    char buf[512];
    size_t total = 0;
    while (fgets(buf, sizeof(buf), f)) total += strlen(buf);
    HU_ASSERT_GT(total, 0u);

    alloc.free(alloc.ctx, run.suite_name, strlen(run.suite_name) + 1);
    fclose(f);
}

static void dashboard_render_no_runs(void) {
    hu_allocator_t alloc = hu_system_allocator();
    FILE *f = tmpfile();
    HU_ASSERT(f != NULL);
    hu_error_t err = hu_eval_dashboard_render(&alloc, f, NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
    rewind(f);
    char buf[256];
    HU_ASSERT(fgets(buf, sizeof(buf), f) != NULL);
    fclose(f);
}

void run_eval_benchmarks_tests(void) {
    HU_TEST_SUITE("eval_bench");
    HU_RUN_TEST(benchmark_load_gaia_format);
    HU_RUN_TEST(benchmark_load_swe_bench_format);
    HU_RUN_TEST(benchmark_load_tool_use_format);
    HU_RUN_TEST(benchmark_type_name_returns_correct);
    HU_RUN_TEST(dashboard_render_single_run);
    HU_RUN_TEST(dashboard_render_no_runs);
}
