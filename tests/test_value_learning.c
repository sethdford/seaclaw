/*
 * Tests for src/intelligence/value_learning.c — value inference from corrections/approvals.
 * Requires HU_ENABLE_SQLITE.
 */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/intelligence/value_learning.h"
#include "test_framework.h"
#include <sqlite3.h>
#include <string.h>

#ifdef HU_ENABLE_SQLITE

static sqlite3 *open_test_db(void) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    return db;
}

static void close_test_db(sqlite3 *db) {
    if (db)
        sqlite3_close(db);
}

static void test_value_engine_create_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_value_engine_t engine = {0};
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_value_engine_create(NULL, db, &engine), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_value_engine_create(&alloc, NULL, &engine), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_value_engine_create(&alloc, db, NULL), HU_ERR_INVALID_ARGUMENT);
    close_test_db(db);
}

static void test_value_engine_create_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    HU_ASSERT_EQ(hu_value_engine_create(&alloc, db, &engine), HU_OK);
    HU_ASSERT_EQ(engine.db, db);
    close_test_db(db);
}

static void test_value_init_tables(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    HU_ASSERT_EQ(hu_value_init_tables(&engine), HU_OK);
    HU_ASSERT_EQ(hu_value_init_tables(&engine), HU_OK);
    close_test_db(db);
}

static void test_value_learn_from_correction(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "privacy", 7, "user cares about privacy", 23, 0.8, 1000), HU_OK);

    hu_value_t val = {0};
    bool found = false;
    HU_ASSERT_EQ(hu_value_get(&engine, "privacy", 7, &val, &found), HU_OK);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_TRUE(val.importance > 0.0);

    close_test_db(db);
}

static void test_value_learn_from_correction_update(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "honesty", 7, "be honest", 9, 0.3, 1000), HU_OK);
    hu_value_t val1 = {0};
    bool found1 = false;
    hu_value_get(&engine, "honesty", 7, &val1, &found1);
    HU_ASSERT_TRUE(found1);
    int32_t ev1 = val1.evidence_count;

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "honesty", 7, "be honest", 9, 0.9, 1001), HU_OK);
    hu_value_t val2 = {0};
    bool found2 = false;
    hu_value_get(&engine, "honesty", 7, &val2, &found2);
    HU_ASSERT_TRUE(found2);
    HU_ASSERT_GT(val2.evidence_count, ev1);

    close_test_db(db);
}

static void test_value_learn_from_approval(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_approval(&engine, "transparency", 12, 0.9, 1000), HU_OK);

    hu_value_t val = {0};
    bool found = false;
    HU_ASSERT_EQ(hu_value_get(&engine, "transparency", 12, &val, &found), HU_OK);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_TRUE(val.importance > 0.0);

    close_test_db(db);
}

static void test_value_weaken(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "speed", 5, "fast", 4, 0.6, 1000), HU_OK);
    hu_value_t val_before = {0};
    bool found = false;
    hu_value_get(&engine, "speed", 5, &val_before, &found);
    HU_ASSERT_TRUE(found);

    HU_ASSERT_EQ(hu_value_weaken(&engine, "speed", 5, 0.2, 1001), HU_OK);
    hu_value_t val_after = {0};
    found = false;
    hu_value_get(&engine, "speed", 5, &val_after, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_TRUE(val_after.importance < val_before.importance);

    close_test_db(db);
}

static void test_value_weaken_deletes_at_threshold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "ephemeral", 9, "temp", 4, 0.2, 1000), HU_OK);
    HU_ASSERT_EQ(hu_value_weaken(&engine, "ephemeral", 9, 1.0, 1001), HU_OK);

    hu_value_t val = {0};
    bool found = false;
    hu_value_get(&engine, "ephemeral", 9, &val, &found);
    HU_ASSERT_FALSE(found);

    close_test_db(db);
}

static void test_value_list(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "a", 1, "desc", 4, 0.3, 1000), HU_OK);
    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "b", 1, "desc", 4, 0.3, 1001), HU_OK);
    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "c", 1, "desc", 4, 0.9, 1002), HU_OK);

    hu_value_t *out = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_value_list(&engine, &out, &count), HU_OK);
    HU_ASSERT_EQ(count, 3u);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out[0].importance >= out[1].importance);
    HU_ASSERT_TRUE(out[1].importance >= out[2].importance);
    hu_value_free(&alloc, out, count);
    close_test_db(db);
}

static void test_value_count(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "x", 1, NULL, 0, 0.5, 1000), HU_OK);
    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "y", 1, NULL, 0, 0.5, 1001), HU_OK);

    size_t n = 0;
    HU_ASSERT_EQ(hu_value_count(&engine, &n), HU_OK);
    HU_ASSERT_EQ(n, 2u);

    close_test_db(db);
}

static void test_value_build_prompt(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "quality", 7, "high quality", 11, 0.7, 1000), HU_OK);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_value_build_prompt(&engine, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(out, "quality") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    close_test_db(db);
}

static void test_value_build_prompt_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_value_build_prompt(&engine, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
    HU_ASSERT_EQ(out[0], '\0');
    alloc.free(alloc.ctx, out, 1);
    close_test_db(db);
}

static void test_value_alignment_score(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "privacy", 7, "protect data", 11, 0.8, 1000), HU_OK);

    hu_value_t *values = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_value_list(&engine, &values, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);

    double score = hu_value_alignment_score(values, count, "ensure privacy for users", 24);
    HU_ASSERT_GT(score, 0.0);

    hu_value_free(&alloc, values, count);
    close_test_db(db);
}

static void test_value_alignment_score_no_match(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_value_engine_t engine = {0};
    hu_value_engine_create(&alloc, db, &engine);
    hu_value_init_tables(&engine);

    HU_ASSERT_EQ(hu_value_learn_from_correction(&engine, "privacy", 7, "protect data", 11, 0.8, 1000), HU_OK);

    hu_value_t *values = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_value_list(&engine, &values, &count), HU_OK);

    double score = hu_value_alignment_score(values, count, "deploy to production", 20);
    HU_ASSERT_EQ(score, 0.0);

    hu_value_free(&alloc, values, count);
    close_test_db(db);
}

static void test_value_free_handles_null(void) {
    hu_value_free(NULL, NULL, 0);
}

#endif /* HU_ENABLE_SQLITE */

void run_value_learning_tests(void) {
#ifdef HU_ENABLE_SQLITE
    HU_TEST_SUITE("value_learning");
    HU_RUN_TEST(test_value_engine_create_null_args);
    HU_RUN_TEST(test_value_engine_create_ok);
    HU_RUN_TEST(test_value_init_tables);
    HU_RUN_TEST(test_value_learn_from_correction);
    HU_RUN_TEST(test_value_learn_from_correction_update);
    HU_RUN_TEST(test_value_learn_from_approval);
    HU_RUN_TEST(test_value_weaken);
    HU_RUN_TEST(test_value_weaken_deletes_at_threshold);
    HU_RUN_TEST(test_value_list);
    HU_RUN_TEST(test_value_count);
    HU_RUN_TEST(test_value_build_prompt);
    HU_RUN_TEST(test_value_build_prompt_empty);
    HU_RUN_TEST(test_value_alignment_score);
    HU_RUN_TEST(test_value_alignment_score_no_match);
    HU_RUN_TEST(test_value_free_handles_null);
#endif
}
