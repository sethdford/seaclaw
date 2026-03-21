#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/eval.h"
#include "test_framework.h"
#include <stdbool.h>
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

static void test_eval_improvement_exceeds_10_percent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);

    hu_eval_run_t run1 = {0};
    run1.suite_name = (char *)"audit-suite";
    run1.passed = 5;
    run1.failed = 5;
    run1.pass_rate = 0.5;
    HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, &run1), HU_OK);

    hu_eval_run_t run2 = {0};
    run2.suite_name = (char *)"audit-suite";
    run2.passed = 13;
    run2.failed = 7;
    run2.pass_rate = 0.65;
    HU_ASSERT_EQ(hu_eval_store_run(&alloc, db, &run2), HU_OK);

    hu_eval_run_t loaded[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_eval_load_history(&alloc, db, loaded, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 2u);

    /* Improvement from lower to higher pass rate (order may vary with same-second inserts) */
    double a = loaded[0].pass_rate;
    double b = loaded[1].pass_rate;
    double lower = a < b ? a : b;
    double higher = a > b ? a : b;
    double improvement = (higher - lower) / lower;
    HU_ASSERT_TRUE(improvement > 0.10);

    hu_eval_run_free(&alloc, &loaded[0]);
    hu_eval_run_free(&alloc, &loaded[1]);
    sqlite3_close(db);
}

static void test_eval_baselines_table_created(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);
    sqlite3_stmt *st = NULL;
    HU_ASSERT_EQ(sqlite3_prepare_v2(db,
                                    "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='eval_baselines'",
                                    -1, &st, NULL),
                 SQLITE_OK);
    HU_ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
    HU_ASSERT_EQ(sqlite3_column_int(st, 0), 1);
    sqlite3_finalize(st);
    sqlite3_close(db);
}

static void test_eval_persist_get_baseline_roundtrip(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);
    HU_ASSERT_EQ(hu_eval_persist_baseline(db, "eval_baseline_rt_suite", 0.81, 7), HU_OK);
    double got = 0.0;
    HU_ASSERT_EQ(hu_eval_get_baseline(db, "eval_baseline_rt_suite", &got), HU_OK);
    HU_ASSERT_FLOAT_EQ(got, 0.81, 0.001);
    sqlite3_close(db);
}

static void test_eval_get_baseline_unknown_returns_not_found(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    HU_ASSERT_EQ(hu_eval_init_tables(db), HU_OK);
    double x = 99.0;
    HU_ASSERT_EQ(hu_eval_get_baseline(db, "eval_baseline_missing_suite_key", &x), HU_ERR_NOT_FOUND);
    HU_ASSERT_FLOAT_EQ(x, 0.0, 0.001);
    sqlite3_close(db);
}

static void test_eval_persist_get_baseline_null_args(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    double s = 0.0;
    HU_ASSERT_EQ(hu_eval_get_baseline(db, "x", NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_get_baseline(db, NULL, &s), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_persist_baseline(db, NULL, 0.5, 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_persist_baseline(db, "", 0.5, 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_persist_baseline(db, "badscore", -0.1, 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_persist_baseline(db, "badscore2", 1.5, 1), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void test_eval_regression_check_baseline_drop_null_args(void) {
    bool reg = true;
    char msg[128];
    HU_ASSERT_EQ(hu_eval_regression_check_baseline_drop(NULL, NULL, 0.5, 0.1, &reg, msg, sizeof(msg)),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_regression_check_baseline_drop(NULL, "", 0.5, 0.1, &reg, msg, sizeof(msg)),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_regression_check_baseline_drop(NULL, "x", 0.5, 0.1, NULL, msg, sizeof(msg)),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_eval_regression_check_baseline_drop_no_prior_passes(void) {
    bool reg = true;
    char msg[256];
    HU_ASSERT_EQ(
        hu_eval_regression_check_baseline_drop(NULL, "hu_rdrop_noprior_xyz", 0.55, 0.10, &reg, msg, sizeof(msg)),
        HU_OK);
    HU_ASSERT_TRUE(!reg);
}

static void test_eval_regression_check_baseline_drop_detects_steep_decline(void) {
    HU_ASSERT_EQ(hu_eval_persist_baseline(NULL, "hu_rdrop_decline_suite", 0.58, 8), HU_OK);
    bool reg = false;
    char msg[256];
    HU_ASSERT_EQ(hu_eval_regression_check_baseline_drop(NULL, "hu_rdrop_decline_suite", 0.45, 0.10, &reg, msg,
                                                        sizeof(msg)),
                 HU_OK);
    HU_ASSERT_TRUE(reg);
    HU_ASSERT_TRUE(strstr(msg, "FAIL:") != NULL);
    HU_ASSERT_TRUE(strstr(msg, "hu_rdrop_decline_suite") != NULL);
    HU_ASSERT_TRUE(strstr(msg, "0.58") != NULL);
    HU_ASSERT_TRUE(strstr(msg, "0.45") != NULL);
    HU_ASSERT_TRUE(strstr(msg, "(") != NULL);
}

static void test_eval_regression_check_baseline_drop_ten_point_boundary_ok(void) {
    HU_ASSERT_EQ(hu_eval_persist_baseline(NULL, "hu_rdrop_boundary_suite", 0.58, 8), HU_OK);
    bool reg = true;
    char msg[256];
    HU_ASSERT_EQ(hu_eval_regression_check_baseline_drop(NULL, "hu_rdrop_boundary_suite", 0.48, 0.10, &reg, msg,
                                                        sizeof(msg)),
                 HU_OK);
    HU_ASSERT_TRUE(!reg);
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
    HU_RUN_TEST(test_eval_improvement_exceeds_10_percent);
    HU_RUN_TEST(test_eval_baselines_table_created);
    HU_RUN_TEST(test_eval_persist_get_baseline_roundtrip);
    HU_RUN_TEST(test_eval_get_baseline_unknown_returns_not_found);
    HU_RUN_TEST(test_eval_persist_get_baseline_null_args);
    HU_RUN_TEST(test_eval_regression_check_baseline_drop_null_args);
    HU_RUN_TEST(test_eval_regression_check_baseline_drop_no_prior_passes);
    HU_RUN_TEST(test_eval_regression_check_baseline_drop_detects_steep_decline);
    HU_RUN_TEST(test_eval_regression_check_baseline_drop_ten_point_boundary_ok);
}

#else

void run_eval_history_tests(void) {
    (void)0;
}

#endif
