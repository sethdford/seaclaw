#include "test_framework.h"

#ifdef HU_ENABLE_SQLITE
#include "human/experience.h"
#include "human/core/allocator.h"
#include <sqlite3.h>
#include <string.h>

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

static void experience_init_creates_table(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, db), HU_OK);
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, db), HU_OK);
    close_test_db(db);
}

static void experience_record_stores_entry(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, db), HU_OK);
    HU_ASSERT_EQ(hu_experience_record_db(&alloc, db, "fix bug", 7, "resolved", 8, 0.9,
                                      "check logs first", 16), HU_OK);
    hu_experience_entry_t results[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_experience_recall_db(&alloc, db, "fix", 3, results, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_TRUE(results[0].task_len >= 7);
    HU_ASSERT_TRUE(memcmp(results[0].task, "fix bug", 7) == 0);
    HU_ASSERT_TRUE(results[0].score >= 0.89 && results[0].score <= 0.91);
    close_test_db(db);
}

static void experience_recall_matches_keyword(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, db), HU_OK);
    HU_ASSERT_EQ(hu_experience_record_db(&alloc, db, "debug crash", 11, "fixed", 5, 0.8,
                                      "", 0), HU_OK);
    HU_ASSERT_EQ(hu_experience_record_db(&alloc, db, "optimize query", 14, "faster", 6, 0.95,
                                      "", 0), HU_OK);
    hu_experience_entry_t results[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_experience_recall_db(&alloc, db, "debug", 5, results, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_TRUE(strstr(results[0].task, "debug") != NULL);
    close_test_db(db);
}

static void experience_recall_empty_store(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, db), HU_OK);
    hu_experience_entry_t results[4];
    size_t count = 99;
    HU_ASSERT_EQ(hu_experience_recall_db(&alloc, db, "anything", 8, results, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 0u);
    close_test_db(db);
}

static void experience_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    hu_experience_entry_t results[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_experience_init_tables(NULL, db), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_record_db(&alloc, NULL, "t", 1, "o", 1, 0.5, "", 0),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_record_db(&alloc, db, NULL, 1, "o", 1, 0.5, "", 0),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_record_db(&alloc, db, "t", 1, NULL, 1, 0.5, "", 0),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_recall_db(&alloc, db, "q", 1, NULL, 4, &count),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_experience_recall_db(&alloc, db, "q", 1, results, 4, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    close_test_db(db);
}

static void experience_limits_results(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = open_test_db();
    HU_ASSERT_EQ(hu_experience_init_tables(&alloc, db), HU_OK);
    for (int i = 0; i < 5; i++) {
        char task[32];
        int n = (int)snprintf(task, sizeof(task), "task%d", i);
        HU_ASSERT_EQ(hu_experience_record_db(&alloc, db, task, (size_t)n, "ok", 2, 0.5 + i * 0.1,
                     "", 0), HU_OK);
    }
    hu_experience_entry_t results[2];
    size_t count = 0;
    HU_ASSERT_EQ(hu_experience_recall_db(&alloc, db, "task", 4, results, 2, &count), HU_OK);
    HU_ASSERT_EQ(count, 2u);
    close_test_db(db);
}

void run_experience_engine_tests(void) {
    HU_TEST_SUITE("Experience Engine");
    HU_RUN_TEST(experience_init_creates_table);
    HU_RUN_TEST(experience_record_stores_entry);
    HU_RUN_TEST(experience_recall_matches_keyword);
    HU_RUN_TEST(experience_recall_empty_store);
    HU_RUN_TEST(experience_null_args_returns_error);
    HU_RUN_TEST(experience_limits_results);
}

#else
void run_experience_engine_tests(void) { (void)0; }
#endif /* HU_ENABLE_SQLITE */
