#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/eval.h"
#include "test_framework.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

static void test_eval_init_tables_succeeds(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_NOT_NULL(db);
    hu_error_t err = hu_eval_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);
    sqlite3_close(db);
}

static void test_eval_store_and_load_run(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    hu_eval_run_t run = {0};
    run.suite_name = hu_strdup(&alloc, "test-suite");
    HU_ASSERT_NOT_NULL(run.suite_name);
    run.passed = 2;
    run.failed = 1;
    run.pass_rate = 2.0 / 3.0;
    run.total_elapsed_ms = 100;
    run.results_count = 2;
    run.results = alloc.alloc(alloc.ctx, 2 * sizeof(hu_eval_result_t));
    HU_ASSERT_NOT_NULL(run.results);
    memset(run.results, 0, 2 * sizeof(hu_eval_result_t));
    run.results[0].task_id = alloc.alloc(alloc.ctx, 4);
    if (run.results[0].task_id) memcpy(run.results[0].task_id, "t1", 3);
    run.results[0].passed = true;
    run.results[0].score = 1.0;
    run.results[1].task_id = alloc.alloc(alloc.ctx, 4);
    if (run.results[1].task_id) memcpy(run.results[1].task_id, "t2", 3);
    run.results[1].passed = false;
    run.results[1].score = 0.5;

    HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, &run), HU_OK);

    hu_eval_run_t loaded[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, db, loaded, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_NOT_NULL(loaded[0].suite_name);
    HU_ASSERT_STR_EQ(loaded[0].suite_name, "test-suite");
    HU_ASSERT_EQ(loaded[0].passed, 2u);
    HU_ASSERT_EQ(loaded[0].failed, 1u);
    HU_ASSERT_FLOAT_EQ(loaded[0].pass_rate, 2.0 / 3.0, 0.001);
    HU_ASSERT_EQ(loaded[0].total_elapsed_ms, 100);

    hu_eval_run_free(&alloc, &loaded[0]);
    hu_eval_run_free(&alloc, &run);
    sqlite3_close(db);
}

static void test_eval_load_empty_history(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    hu_eval_run_t runs[4];
    size_t count = 99;
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, db, runs, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 0u);
    sqlite3_close(db);
}

static void test_eval_store_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_eval_run_t run = {0};
    run.suite_name = (char *)"x";
    run.passed = 1;
    run.failed = 0;
    run.pass_rate = 1.0;

    HU_ASSERT_EQ(hu_eval_store_run(NULL, db, &run), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_store_run(&alloc, NULL, &run), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, NULL), HU_ERR_INVALID_ARGUMENT);

    size_t count = 0;
    hu_eval_run_t runs[1];
    HU_ASSERT_EQ(hu_eval_load_history(NULL, db, runs, 1, &count), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, NULL, runs, 1, &count), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, db, NULL, 1, &count), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, db, runs, 1, NULL), HU_ERR_INVALID_ARGUMENT);

    HU_ASSERT_EQ(hu_eval_init_tables(NULL), HU_ERR_INVALID_ARGUMENT);

    sqlite3_close(db);
}

static void test_eval_store_with_provider_and_model(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    hu_eval_run_t run = {0};
    run.suite_name = (char *)"reasoning";
    run.provider = (char *)"gemini";
    run.model = (char *)"gemini-2.5-pro";
    run.passed = 8;
    run.failed = 2;
    run.pass_rate = 0.80;

    HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, &run), HU_OK);

    hu_eval_run_t loaded[1];
    size_t count = 0;
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, db, loaded, 1, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_NOT_NULL(loaded[0].provider);
    HU_ASSERT_STR_EQ(loaded[0].provider, "gemini");
    HU_ASSERT_NOT_NULL(loaded[0].model);
    HU_ASSERT_STR_EQ(loaded[0].model, "gemini-2.5-pro");

    hu_eval_run_free(&alloc, &loaded[0]);
    sqlite3_close(db);
}

static void test_eval_detect_regression_null_args(void) {
    hu_eval_regression_t reg = {0};
    HU_ASSERT_EQ(hu_eval_detect_regression(NULL, NULL, 0.8, 0.05, &reg), HU_ERR_INVALID_ARGUMENT);
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_detect_regression(db, NULL, 0.8, 0.05, NULL), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void test_eval_detect_regression_no_history(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    hu_eval_regression_t reg = {0};
    HU_ASSERT_EQ(hu_eval_detect_regression(db, NULL, 0.90, 0.05, &reg), HU_OK);
    HU_ASSERT_EQ(reg.baseline_runs, 0u);
    HU_ASSERT_TRUE(!reg.regressed);
    sqlite3_close(db);
}

static void test_eval_detect_regression_passes_when_stable(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    for (int i = 0; i < 5; i++) {
        hu_eval_run_t r = {0};
        r.suite_name = (char *)"core";
        r.passed = 9;
        r.failed = 1;
        r.pass_rate = 0.90;
        HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, &r), HU_OK);
    }

    hu_eval_regression_t reg = {0};
    HU_ASSERT_EQ(hu_eval_detect_regression(db, "core", 0.88, 0.05, &reg), HU_OK);
    HU_ASSERT_TRUE(!reg.regressed);
    HU_ASSERT_EQ(reg.baseline_runs, 5u);
    HU_ASSERT_FLOAT_EQ(reg.baseline_pass_rate, 0.90, 0.001);
    sqlite3_close(db);
}

static void test_eval_detect_regression_triggers_on_drop(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    for (int i = 0; i < 5; i++) {
        hu_eval_run_t r = {0};
        r.suite_name = (char *)"core";
        r.passed = 9;
        r.failed = 1;
        r.pass_rate = 0.90;
        HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, &r), HU_OK);
    }

    hu_eval_regression_t reg = {0};
    HU_ASSERT_EQ(hu_eval_detect_regression(db, "core", 0.80, 0.05, &reg), HU_OK);
    HU_ASSERT_TRUE(reg.regressed);
    HU_ASSERT_TRUE(reg.delta < -0.05);
    sqlite3_close(db);
}

void run_eval_history_tests(void) {
    HU_TEST_SUITE("Eval History Storage");
    HU_RUN_TEST(test_eval_init_tables_succeeds);
    HU_RUN_TEST(test_eval_store_and_load_run);
    HU_RUN_TEST(test_eval_load_empty_history);
    HU_RUN_TEST(test_eval_store_null_args_returns_error);
    HU_RUN_TEST(test_eval_store_with_provider_and_model);
    HU_RUN_TEST(test_eval_detect_regression_null_args);
    HU_RUN_TEST(test_eval_detect_regression_no_history);
    HU_RUN_TEST(test_eval_detect_regression_passes_when_stable);
    HU_RUN_TEST(test_eval_detect_regression_triggers_on_drop);
}

#else

void run_eval_history_tests(void) {
    (void)0;
}

#endif
