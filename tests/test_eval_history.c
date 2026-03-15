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

void run_eval_history_tests(void) {
    HU_TEST_SUITE("Eval History Storage");
    HU_RUN_TEST(test_eval_init_tables_succeeds);
    HU_RUN_TEST(test_eval_store_and_load_run);
    HU_RUN_TEST(test_eval_load_empty_history);
    HU_RUN_TEST(test_eval_store_null_args_returns_error);
}

#else

void run_eval_history_tests(void) {
    (void)0;
}

#endif
