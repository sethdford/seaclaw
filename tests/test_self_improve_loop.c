#include "test_framework.h"
#ifdef HU_ENABLE_SQLITE
#include "human/intelligence/self_improve.h"
#include "human/intelligence/weakness.h"
#include "human/eval.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static sqlite3 *open_mem_db(void) {
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    return db;
}

static void setup_eval_patches_table(sqlite3 *db) {
    const char *ddl =
        "CREATE TABLE IF NOT EXISTS eval_patches("
        "id INTEGER PRIMARY KEY, weakness_type TEXT, task_id TEXT, "
        "patch_text TEXT, applied INTEGER DEFAULT 0, "
        "pass_rate_before REAL, pass_rate_after REAL, "
        "kept INTEGER DEFAULT 0, created_at INTEGER)";
    sqlite3_exec(db, ddl, NULL, NULL, NULL);
}

static void test_from_assessment_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);
    hu_eval_run_t run = {0};

    HU_ASSERT_EQ(hu_self_improve_from_assessment(NULL, &run, NULL, 1000), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_self_improve_from_assessment(&engine, NULL, NULL, 1000), HU_ERR_INVALID_ARGUMENT);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_from_assessment_empty_run(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);

    hu_eval_run_t run = {.results = NULL, .results_count = 0, .passed = 0, .failed = 0};
    HU_ASSERT_EQ(hu_self_improve_from_assessment(&engine, &run, NULL, 1000), HU_OK);

    size_t count = 99;
    hu_self_improve_active_patch_count(&engine, &count);
    HU_ASSERT_EQ(count, 0u);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_self_improve_produces_at_least_one_patch(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    HU_ASSERT_EQ(hu_self_improve_create(&alloc, db, &engine), HU_OK);
    HU_ASSERT_EQ(hu_self_improve_init_tables(&engine), HU_OK);

    hu_eval_task_t tasks[1] = {
        {.id = "t1", .prompt = "2+2", .prompt_len = 3, .expected = "4", .expected_len = 1, .category = "math"},
    };
    hu_eval_suite_t suite = {.name = "audit", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "5", .actual_output_len = 1},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1, .pass_rate = 0.0};

    HU_ASSERT_EQ(hu_self_improve_from_assessment(&engine, &run, &suite, 1000), HU_OK);

    size_t count = 0;
    HU_ASSERT_EQ(hu_self_improve_active_patch_count(&engine, &count), HU_OK);
    HU_ASSERT_TRUE(count > 0u);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_from_assessment_generates_patches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);

    hu_eval_task_t tasks[2] = {
        {.id = "t1", .prompt = "2+2", .prompt_len = 3, .expected = "4", .expected_len = 1, .category = "math"},
        {.id = "t2", .prompt = "cap", .prompt_len = 3, .expected = "Paris", .expected_len = 5, .category = "geography"},
    };
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 2};
    hu_eval_result_t results[2] = {
        {.task_id = "t1", .passed = false, .actual_output = "5", .actual_output_len = 1},
        {.task_id = "t2", .passed = false, .actual_output = "London", .actual_output_len = 6},
    };
    hu_eval_run_t run = {.results = results, .results_count = 2, .failed = 2, .pass_rate = 0.0};

    HU_ASSERT_EQ(hu_self_improve_from_assessment(&engine, &run, &suite, 2000), HU_OK);

    size_t count = 0;
    hu_self_improve_active_patch_count(&engine, &count);
    HU_ASSERT_EQ(count, 2u);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_verify_patch_keeps(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);

    hu_eval_task_t tasks[1] = {
        {.id = "t1", .prompt = "q", .prompt_len = 1, .expected = "a", .expected_len = 1, .category = "math"},
    };
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "b", .actual_output_len = 1},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1, .pass_rate = 0.0};
    hu_self_improve_from_assessment(&engine, &run, &suite, 3000);

    HU_ASSERT_EQ(hu_self_improve_verify_patch(&engine, 1, 0.8), HU_OK);

    size_t kept = 0;
    hu_self_improve_kept_patch_count(&engine, &kept);
    HU_ASSERT_EQ(kept, 1u);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_verify_patch_rollback_on_worse(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);

    hu_eval_task_t tasks[1] = {
        {.id = "t1", .prompt = "q", .prompt_len = 1, .expected = "a", .expected_len = 1, .category = "math"},
    };
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "b", .actual_output_len = 1},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1, .pass_rate = 0.5};
    hu_self_improve_from_assessment(&engine, &run, &suite, 4000);

    HU_ASSERT_EQ(hu_self_improve_verify_patch(&engine, 1, 0.2), HU_OK);

    size_t kept = 99;
    hu_self_improve_kept_patch_count(&engine, &kept);
    HU_ASSERT_EQ(kept, 0u);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_rollback_patch(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);

    hu_eval_task_t tasks[1] = {
        {.id = "t1", .prompt = "q", .prompt_len = 1, .expected = "a", .expected_len = 1, .category = "math"},
    };
    hu_eval_suite_t suite = {.name = "test", .tasks = tasks, .tasks_count = 1};
    hu_eval_result_t results[1] = {
        {.task_id = "t1", .passed = false, .actual_output = "b", .actual_output_len = 1},
    };
    hu_eval_run_t run = {.results = results, .results_count = 1, .failed = 1, .pass_rate = 0.0};
    hu_self_improve_from_assessment(&engine, &run, &suite, 5000);

    hu_self_improve_verify_patch(&engine, 1, 0.8);
    size_t kept = 0;
    hu_self_improve_kept_patch_count(&engine, &kept);
    HU_ASSERT_EQ(kept, 1u);

    HU_ASSERT_EQ(hu_self_improve_rollback_patch(&engine, 1), HU_OK);
    hu_self_improve_kept_patch_count(&engine, &kept);
    HU_ASSERT_EQ(kept, 0u);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_verify_patch_not_found(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    hu_self_improve_create(&alloc, db, &engine);
    hu_self_improve_init_tables(&engine);
    setup_eval_patches_table(db);

    HU_ASSERT_EQ(hu_self_improve_verify_patch(&engine, 9999, 0.5), HU_ERR_NOT_FOUND);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_verify_null_args(void) {
    HU_ASSERT_EQ(hu_self_improve_verify_patch(NULL, 1, 0.5), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_self_improve_rollback_patch(NULL, 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_self_improve_kept_patch_count(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_closed_loop_null_alloc_returns_error(void) {
    sqlite3 *db = open_mem_db();
    HU_ASSERT_EQ(hu_self_improve_closed_loop(NULL, db, NULL, NULL, 0, "suite.json"),
                 HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void test_closed_loop_simulated_improvement_keeps_patch(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    HU_ASSERT_EQ(hu_self_improve_closed_loop(&alloc, db, NULL, NULL, 0, "suite.json"), HU_OK);

    hu_self_improve_t engine = {0};
    HU_ASSERT_EQ(hu_self_improve_create(&alloc, db, &engine), HU_OK);
    size_t kept = 0;
    HU_ASSERT_EQ(hu_self_improve_kept_patch_count(&engine, &kept), HU_OK);
    HU_ASSERT_EQ(kept, 1u);
    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_self_improve_parse_patch_valid_key_value(void) {
    hu_structured_patch_t p = {0};
    const char *s = "TEMPERATURE:0.75";
    HU_ASSERT_TRUE(hu_self_improve_parse_patch(s, strlen(s), &p));
    HU_ASSERT_TRUE(p.parsed);
    HU_ASSERT_EQ((int)p.type, (int)HU_PATCH_TEMPERATURE);
    HU_ASSERT_STR_EQ(p.key, "TEMPERATURE");
    HU_ASSERT_EQ(p.numeric_value, 0.75);
}

static void test_self_improve_parse_patch_malformed_rejected(void) {
    hu_structured_patch_t p = {0};
    HU_ASSERT_FALSE(hu_self_improve_parse_patch("TEMPERATURE", 11, &p));
    /* JSON-like blobs still contain ':'; use bracket-only text so parse fails (no colon). */
    HU_ASSERT_FALSE(hu_self_improve_parse_patch("[1,2,3]", 7, &p));
    HU_ASSERT_FALSE(hu_self_improve_parse_patch("TEMPERATURE:", 12, &p));
}

static void test_self_improve_parse_patch_null_or_empty_rejected(void) {
    hu_structured_patch_t p = {0};
    HU_ASSERT_FALSE(hu_self_improve_parse_patch(NULL, 4, &p));
    HU_ASSERT_FALSE(hu_self_improve_parse_patch("x", 0, &p));
    HU_ASSERT_FALSE(hu_self_improve_parse_patch("TEMPERATURE:1", 13, NULL));
}

static void test_self_improve_apply_structured_patch_under_is_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    HU_ASSERT_EQ(hu_self_improve_create(&alloc, db, &engine), HU_OK);
#if !(defined(HU_IS_TEST) && HU_IS_TEST)
    HU_ASSERT_EQ(hu_self_improve_init_tables(&engine), HU_OK);
#endif

    hu_structured_patch_t patch = {0};
    patch.parsed = true;
    patch.type = HU_PATCH_TEMPERATURE;
    memcpy(patch.key, "TEMPERATURE", sizeof("TEMPERATURE"));
    memcpy(patch.value, "0.5", sizeof("0.5"));
    patch.numeric_value = 0.5;

    HU_ASSERT_EQ(hu_self_improve_apply_structured_patch(&engine, &patch), HU_OK);

    patch.parsed = false;
    HU_ASSERT_EQ(hu_self_improve_apply_structured_patch(&engine, &patch), HU_ERR_INVALID_ARGUMENT);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

static void test_self_improve_eval_and_apply_returns_positive_delta_in_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_structured_patch_t p = {0};
    p.parsed = true;
    p.type = HU_PATCH_TEMPERATURE;
    hu_self_improve_delta_t d = {0};
    HU_ASSERT_EQ(hu_self_improve_eval_and_apply(&alloc, db, &p, &d), HU_OK);
    HU_ASSERT_FLOAT_EQ(d.score_before, 0.7, 1e-9);
    HU_ASSERT_FLOAT_EQ(d.score_after, 0.72, 1e-9);
    HU_ASSERT_FLOAT_EQ(d.delta, 0.02, 1e-9);
    HU_ASSERT_FALSE(d.should_rollback);
    sqlite3_close(db);
}

static void test_self_improve_rollback_if_negative_should_rollback_does_not_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_delta_t d = {0};
    d.should_rollback = true;
    snprintf(d.patch_id, sizeof(d.patch_id), "1");
    HU_ASSERT_EQ(hu_self_improve_rollback_if_negative(&alloc, db, &d), HU_OK);
    sqlite3_close(db);
}

static void test_self_improve_rollback_if_negative_false_is_noop(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_delta_t d = {0};
    d.should_rollback = false;
    HU_ASSERT_EQ(hu_self_improve_rollback_if_negative(&alloc, db, &d), HU_OK);
    sqlite3_close(db);
}

static void test_self_improve_get_structured_patches_test_mode_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_self_improve_t engine = {0};
    HU_ASSERT_EQ(hu_self_improve_create(&alloc, db, &engine), HU_OK);
#if !(defined(HU_IS_TEST) && HU_IS_TEST)
    HU_ASSERT_EQ(hu_self_improve_init_tables(&engine), HU_OK);
#endif

    hu_structured_patch_t *out = NULL;
    size_t count = 99;
    HU_ASSERT_EQ(hu_self_improve_get_structured_patches(&engine, &alloc, HU_PATCH_TEMPERATURE, &out,
                                                        &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 0u);
    HU_ASSERT_NULL(out);

    hu_self_improve_deinit(&engine);
    sqlite3_close(db);
}

#endif /* HU_ENABLE_SQLITE */

void run_self_improve_loop_tests(void) {
    HU_TEST_SUITE("Self-Improve Closed Loop");
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(test_from_assessment_null_args);
    HU_RUN_TEST(test_from_assessment_empty_run);
    HU_RUN_TEST(test_self_improve_produces_at_least_one_patch);
    HU_RUN_TEST(test_from_assessment_generates_patches);
    HU_RUN_TEST(test_verify_patch_keeps);
    HU_RUN_TEST(test_verify_patch_rollback_on_worse);
    HU_RUN_TEST(test_rollback_patch);
    HU_RUN_TEST(test_verify_patch_not_found);
    HU_RUN_TEST(test_verify_null_args);
    HU_RUN_TEST(test_closed_loop_null_alloc_returns_error);
    HU_RUN_TEST(test_closed_loop_simulated_improvement_keeps_patch);
    HU_RUN_TEST(test_self_improve_parse_patch_valid_key_value);
    HU_RUN_TEST(test_self_improve_parse_patch_malformed_rejected);
    HU_RUN_TEST(test_self_improve_parse_patch_null_or_empty_rejected);
    HU_RUN_TEST(test_self_improve_apply_structured_patch_under_is_test);
    HU_RUN_TEST(test_self_improve_eval_and_apply_returns_positive_delta_in_test_mode);
    HU_RUN_TEST(test_self_improve_rollback_if_negative_should_rollback_does_not_crash);
    HU_RUN_TEST(test_self_improve_rollback_if_negative_false_is_noop);
    HU_RUN_TEST(test_self_improve_get_structured_patches_test_mode_empty);
#endif
}
