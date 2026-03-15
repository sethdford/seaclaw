#include "test_framework.h"
#ifdef HU_ENABLE_SQLITE
#include "human/intelligence/distiller.h"
#include <sqlite3.h>
#include <string.h>

static sqlite3 *open_mem_db(void) {
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    return db;
}

static void insert_experience(sqlite3 *db, const char *task, const char *actions,
                               const char *outcome, double score, int64_t ts) {
    const char *sql =
        "INSERT INTO experience_log (task, actions, outcome, score, recorded_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5)";
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, task, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, actions, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, outcome, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, score);
    sqlite3_bind_int64(stmt, 5, ts);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void test_distiller_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    size_t created = 99;
    HU_ASSERT_EQ(hu_experience_distill(NULL, NULL, 2, 1000, &created), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_distill(&alloc, NULL, 2, 1000, &created), HU_ERR_INVALID_ARGUMENT);
    sqlite3 *db = open_mem_db();
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 2, 1000, NULL), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void test_distiller_init_tables(void) {
    sqlite3 *db = open_mem_db();
    HU_ASSERT_EQ(hu_distiller_init_tables(db), HU_OK);
    HU_ASSERT_EQ(hu_distiller_init_tables(NULL), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void test_distiller_empty_log(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_distiller_init_tables(db);

    size_t created = 99;
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 2, 1000, &created), HU_OK);
    HU_ASSERT_EQ(created, 0u);

    sqlite3_close(db);
}

static void test_distiller_single_entry_no_lesson(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_distiller_init_tables(db);

    insert_experience(db, "debug network issue", "checked logs", "resolved timeout", 0.9, 1000);

    size_t created = 99;
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 3, 2000, &created), HU_OK);
    HU_ASSERT_EQ(created, 0u);

    sqlite3_close(db);
}

static void test_distiller_repeated_themes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_distiller_init_tables(db);

    insert_experience(db, "debugging network timeout", "checked logs", "fixed timeout", 0.8, 100);
    insert_experience(db, "debugging network latency", "traced network", "improved latency", 0.9, 200);
    insert_experience(db, "debugging network errors", "reviewed network", "resolved errors", 0.7, 300);

    size_t created = 0;
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 2, 1000, &created), HU_OK);
    HU_ASSERT_TRUE(created > 0);

    /* Verify lessons exist in table */
    const char *sel = "SELECT COUNT(*) FROM general_lessons WHERE source = 'distiller'";
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(db, sel, -1, &stmt, NULL);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    HU_ASSERT_TRUE(count > 0);

    sqlite3_close(db);
}

static void test_distiller_min_occurrences_threshold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_distiller_init_tables(db);

    insert_experience(db, "one unique snowflake word", "action1", "outcome1", 0.5, 100);
    insert_experience(db, "another completely different text", "action2", "outcome2", 0.5, 200);

    size_t created = 0;
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 5, 1000, &created), HU_OK);
    HU_ASSERT_EQ(created, 0u);

    sqlite3_close(db);
}

static void test_distiller_deduplicates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_mem_db();
    hu_distiller_init_tables(db);

    insert_experience(db, "network debugging session", "checked network", "fixed network", 0.8, 100);
    insert_experience(db, "network monitoring task", "monitored network", "network stable", 0.9, 200);
    insert_experience(db, "network analysis report", "analyzed network", "network report done", 0.7, 300);

    size_t created1 = 0;
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 2, 1000, &created1), HU_OK);
    HU_ASSERT_TRUE(created1 > 0);

    /* Running again should not create duplicates */
    size_t created2 = 0;
    HU_ASSERT_EQ(hu_experience_distill(&alloc, db, 2, 2000, &created2), HU_OK);
    HU_ASSERT_EQ(created2, 0u);

    sqlite3_close(db);
}

#endif /* HU_ENABLE_SQLITE */

void run_distiller_tests(void) {
    HU_TEST_SUITE("Experience Distiller");
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(test_distiller_null_args);
    HU_RUN_TEST(test_distiller_init_tables);
    HU_RUN_TEST(test_distiller_empty_log);
    HU_RUN_TEST(test_distiller_single_entry_no_lesson);
    HU_RUN_TEST(test_distiller_repeated_themes);
    HU_RUN_TEST(test_distiller_min_occurrences_threshold);
    HU_RUN_TEST(test_distiller_deduplicates);
#endif
}
