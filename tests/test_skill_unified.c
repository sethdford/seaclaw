#ifdef HU_ENABLE_SQLITE

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/intelligence/skills.h"
#include "human/memory.h"
#include "test_framework.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>

static void ensure_skills_table(sqlite3 *db) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS skills ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "type TEXT NOT NULL,"
        "contact_id TEXT,"
        "trigger_conditions TEXT,"
        "strategy TEXT NOT NULL,"
        "success_rate REAL DEFAULT 0.5,"
        "attempts INTEGER DEFAULT 0,"
        "successes INTEGER DEFAULT 0,"
        "version INTEGER DEFAULT 1,"
        "origin TEXT NOT NULL,"
        "parent_skill_id INTEGER,"
        "created_at INTEGER NOT NULL,"
        "updated_at INTEGER,"
        "retired INTEGER DEFAULT 0)";
    sqlite3_exec(db, sql, NULL, NULL, NULL);
}

static void skill_insert_and_retrieve_parameterized(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_skills_table(db);

    int64_t now = (int64_t)time(NULL);
    int64_t out_id = 0;
    hu_error_t err = hu_skill_insert(&alloc, db,
                                     "param_skill", 11,
                                     "test", 4,
                                     "user_a", 6,
                                     "topic==weather", 15,
                                     "Suggest bringing an umbrella.", 29,
                                     "manual", 6,
                                     0, now, &out_id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out_id > 0);

    hu_skill_t out = {0};
    err = hu_skill_get_by_name(&alloc, db, "param_skill", 11, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out.id, out_id);
    HU_ASSERT_STR_EQ(out.name, "param_skill");
    HU_ASSERT_STR_EQ(out.trigger_conditions, "topic==weather");
    HU_ASSERT_STR_EQ(out.strategy, "Suggest bringing an umbrella.");

    mem.vtable->deinit(mem.ctx);
}

static void skill_discover_from_pattern(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_skills_table(db);

    int64_t out_id = 0;
    hu_error_t err = hu_skill_discover_from_pattern(&alloc, db,
                                                    "user asks about refunds", 24,
                                                    0.85, "refund_handling", 16,
                                                    &out_id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out_id > 0);

    hu_skill_t out = {0};
    err = hu_skill_get_by_name(&alloc, db, "refund_handling", 16, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out.type, "discovered");
    HU_ASSERT_STR_EQ(out.origin, "discovery");
    HU_ASSERT_STR_EQ(out.trigger_conditions, "user asks about refunds");
    HU_ASSERT_STR_EQ(out.strategy, "user asks about refunds");
    HU_ASSERT_EQ(out.success_rate, 0.85);

    mem.vtable->deinit(mem.ctx);
}

static void skill_sql_injection_prevented(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_skills_table(db);

    const char *malicious = "'; DROP TABLE skills; --";
    size_t malicious_len = strlen(malicious);
    int64_t out_id = 0;
    hu_error_t err = hu_skill_discover_from_pattern(&alloc, db,
                                                    malicious, malicious_len,
                                                    0.5, "injection_test", 14,
                                                    &out_id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out_id > 0);

    hu_skill_t out = {0};
    err = hu_skill_get_by_name(&alloc, db, "injection_test", 14, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out.trigger_conditions, malicious);
    HU_ASSERT_STR_EQ(out.strategy, malicious);

    hu_skill_t *skills = NULL;
    size_t count = 0;
    err = hu_skill_load_active(&alloc, db, NULL, 0, &skills, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1u);
    if (skills)
        hu_skill_free(&alloc, skills, count);

    mem.vtable->deinit(mem.ctx);
}

static void skill_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_skills_table(db);

    int64_t out_id = 0;
    hu_error_t err = hu_skill_discover_from_pattern(NULL, db, "p", 1, 0.5, "n", 1, &out_id);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_skill_discover_from_pattern(&alloc, NULL, "p", 1, 0.5, "n", 1, &out_id);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_skill_discover_from_pattern(&alloc, db, NULL, 1, 0.5, "n", 1, &out_id);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_skill_discover_from_pattern(&alloc, db, "p", 1, 0.5, NULL, 1, &out_id);
    HU_ASSERT_NEQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

void run_skill_unified_tests(void) {
    HU_TEST_SUITE("skill_unified");
    HU_RUN_TEST(skill_insert_and_retrieve_parameterized);
    HU_RUN_TEST(skill_discover_from_pattern);
    HU_RUN_TEST(skill_sql_injection_prevented);
    HU_RUN_TEST(skill_null_args_returns_error);
}

#else

void run_skill_unified_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
