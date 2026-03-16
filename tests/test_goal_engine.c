/*
 * Tests for src/agent/goals.c — goal autonomy with decomposition and selection.
 * Requires HU_ENABLE_SQLITE.
 */

#include "human/agent/goals.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
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

static void test_goal_engine_create_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_goal_engine_t engine = {0};
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_goal_engine_create(NULL, db, &engine), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_goal_engine_create(&alloc, NULL, &engine), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_goal_engine_create(&alloc, db, NULL), HU_ERR_INVALID_ARGUMENT);
    close_test_db(db);
}

static void test_goal_engine_create_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    HU_ASSERT_EQ(hu_goal_engine_create(&alloc, db, &engine), HU_OK);
    HU_ASSERT_EQ(engine.db, db);
    close_test_db(db);
}

static void test_goal_init_tables(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    HU_ASSERT_EQ(hu_goal_init_tables(&engine), HU_OK);
    HU_ASSERT_EQ(hu_goal_init_tables(&engine), HU_OK);
    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t out_id = 0;
    HU_ASSERT_EQ(hu_goal_create(&engine, "finish report", 13, 0.7, 0, 0, 1000, &out_id), HU_OK);
    HU_ASSERT_GT(out_id, 0);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_create_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t out_id = 0;
    HU_ASSERT_EQ(hu_goal_create(&engine, NULL, 0, 0.5, 0, 0, 1000, &out_id), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_goal_create(&engine, "ok", 2, 0.5, 0, 0, 1000, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_get(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id = 0;
    hu_goal_create(&engine, "learn Rust", 10, 0.8, 0, 0, 1000, &id);

    hu_goal_t goal = {0};
    bool found = false;
    HU_ASSERT_EQ(hu_goal_get(&engine, id, &goal, &found), HU_OK);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(goal.id, id);
    HU_ASSERT_STR_EQ(goal.description, "learn Rust");
    HU_ASSERT_EQ(goal.status, HU_AUTO_GOAL_PENDING);
    HU_ASSERT_FLOAT_EQ(goal.priority, 0.8, 0.001);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_update_status(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id = 0;
    hu_goal_create(&engine, "task", 4, 0.5, 0, 0, 1000, &id);
    HU_ASSERT_EQ(hu_goal_update_status(&engine, id, HU_AUTO_GOAL_ACTIVE, 1001), HU_OK);

    hu_goal_t goal = {0};
    bool found = false;
    hu_goal_get(&engine, id, &goal, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(goal.status, HU_AUTO_GOAL_ACTIVE);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_update_progress(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id = 0;
    hu_goal_create(&engine, "build feature", 14, 0.6, 0, 0, 1000, &id);
    HU_ASSERT_EQ(hu_goal_update_progress(&engine, id, 0.5, 1001), HU_OK);

    hu_goal_t goal = {0};
    bool found = false;
    hu_goal_get(&engine, id, &goal, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_FLOAT_EQ(goal.progress, 0.5, 0.001);

    HU_ASSERT_EQ(hu_goal_update_progress(&engine, id, 1.0, 1002), HU_OK);
    hu_goal_get(&engine, id, &goal, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(goal.status, HU_AUTO_GOAL_COMPLETED);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_decompose(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t parent_id = 0;
    hu_goal_create(&engine, "parent goal", 11, 0.9, 0, 0, 1000, &parent_id);

    const char *descs[] = {"sub1", "sub2", "sub3"};
    size_t lens[] = {4, 4, 4};
    int64_t ids[3] = {0};
    HU_ASSERT_EQ(hu_goal_decompose(&engine, parent_id, descs, lens, 3, 1001, ids), HU_OK);
    HU_ASSERT_GT(ids[0], 0);
    HU_ASSERT_GT(ids[1], 0);
    HU_ASSERT_GT(ids[2], 0);

    hu_goal_t g = {0};
    bool found = false;
    hu_goal_get(&engine, ids[0], &g, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(g.parent_id, parent_id);
    hu_goal_get(&engine, ids[1], &g, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(g.parent_id, parent_id);
    hu_goal_get(&engine, ids[2], &g, &found);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(g.parent_id, parent_id);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_select_next(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id1 = 0, id2 = 0;
    hu_goal_create(&engine, "low", 3, 0.2, 0, 0, 1000, &id1);
    hu_goal_create(&engine, "high", 4, 0.95, 0, 0, 1001, &id2);

    hu_goal_t next = {0};
    bool found = false;
    HU_ASSERT_EQ(hu_goal_select_next(&engine, &next, &found), HU_OK);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(next.id, id2);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_list_active(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id1 = 0, id2 = 0, id3 = 0;
    hu_goal_create(&engine, "pending", 7, 0.5, 0, 0, 1000, &id1);
    hu_goal_create(&engine, "active", 6, 0.8, 0, 0, 1001, &id2);
    hu_goal_create(&engine, "to complete", 11, 0.9, 0, 0, 1002, &id3);
    hu_goal_update_status(&engine, id2, HU_AUTO_GOAL_ACTIVE, 1003);
    hu_goal_update_progress(&engine, id3, 1.0, 1004);

    hu_goal_t *out = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_goal_list_active(&engine, &out, &count), HU_OK);
    HU_ASSERT_EQ(count, 2u);
    HU_ASSERT_NOT_NULL(out);
    hu_goal_free(&alloc, out, count);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_count(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id = 0;
    hu_goal_create(&engine, "one", 3, 0.5, 0, 0, 1000, &id);
    hu_goal_create(&engine, "two", 3, 0.5, 0, 0, 1001, &id);

    size_t n = 0;
    HU_ASSERT_EQ(hu_goal_count(&engine, &n), HU_OK);
    HU_ASSERT_EQ(n, 2u);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_build_context(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    int64_t id = 0;
    hu_goal_create(&engine, "ship v1", 7, 0.8, 0, 0, 1000, &id);

    char *ctx = NULL;
    size_t ctx_len = 0;
    HU_ASSERT_EQ(hu_goal_build_context(&engine, &ctx, &ctx_len), HU_OK);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(ctx_len > 0);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_free_handles_null(void) {
    hu_goal_free(NULL, NULL, 0);
}

static void test_goal_engine_deinit_null_handles_gracefully(void) {
    hu_goal_engine_deinit(NULL);
}

static void test_goal_select_next_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    hu_goal_t out = {0};
    bool found = false;
    HU_ASSERT_EQ(hu_goal_select_next(NULL, &out, &found), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_goal_select_next(&engine, NULL, &found), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_goal_select_next(&engine, &out, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_goal_count_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_goal_engine_t engine = {0};
    hu_goal_engine_create(&alloc, db, &engine);
    hu_goal_init_tables(&engine);

    size_t n = 0;
    HU_ASSERT_EQ(hu_goal_count(NULL, &n), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_goal_count(&engine, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_goal_engine_deinit(&engine);
    close_test_db(db);
}

static void test_auto_goal_status_str(void) {
    HU_ASSERT_NOT_NULL(hu_auto_goal_status_str(HU_AUTO_GOAL_PENDING));
    HU_ASSERT_NOT_NULL(hu_auto_goal_status_str(HU_AUTO_GOAL_ACTIVE));
    HU_ASSERT_NOT_NULL(hu_auto_goal_status_str(HU_AUTO_GOAL_COMPLETED));
    HU_ASSERT_NOT_NULL(hu_auto_goal_status_str(HU_AUTO_GOAL_BLOCKED));
    HU_ASSERT_NOT_NULL(hu_auto_goal_status_str(HU_AUTO_GOAL_ABANDONED));
}

#endif /* HU_ENABLE_SQLITE */

void run_goal_engine_tests(void) {
#ifdef HU_ENABLE_SQLITE
    HU_TEST_SUITE("goal_engine");
    HU_RUN_TEST(test_goal_engine_create_null_args);
    HU_RUN_TEST(test_goal_engine_create_ok);
    HU_RUN_TEST(test_goal_init_tables);
    HU_RUN_TEST(test_goal_create);
    HU_RUN_TEST(test_goal_create_invalid);
    HU_RUN_TEST(test_goal_get);
    HU_RUN_TEST(test_goal_update_status);
    HU_RUN_TEST(test_goal_update_progress);
    HU_RUN_TEST(test_goal_decompose);
    HU_RUN_TEST(test_goal_select_next);
    HU_RUN_TEST(test_goal_list_active);
    HU_RUN_TEST(test_goal_count);
    HU_RUN_TEST(test_goal_build_context);
    HU_RUN_TEST(test_goal_free_handles_null);
    HU_RUN_TEST(test_goal_engine_deinit_null_handles_gracefully);
    HU_RUN_TEST(test_goal_select_next_null_args);
    HU_RUN_TEST(test_goal_count_null_args);
    HU_RUN_TEST(test_auto_goal_status_str);
#endif
}
