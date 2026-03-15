#ifdef HU_ENABLE_SQLITE

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/feeds/findings.h"
#include "human/feeds/research_executor.h"
#include "human/memory.h"
#include "test_framework.h"
#include <sqlite3.h>
#include <string.h>

static void ensure_tables(sqlite3 *db) {
    const char *findings =
        "CREATE TABLE IF NOT EXISTS research_findings("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT,"
        "finding TEXT NOT NULL,"
        "relevance TEXT,"
        "priority TEXT DEFAULT 'MEDIUM',"
        "suggested_action TEXT,"
        "status TEXT DEFAULT 'pending',"
        "created_at INTEGER NOT NULL,"
        "acted_at INTEGER)";
    sqlite3_exec(db, findings, NULL, NULL, NULL);

    const char *lessons =
        "CREATE TABLE IF NOT EXISTS general_lessons("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "lesson TEXT NOT NULL,"
        "confidence REAL DEFAULT 0.5,"
        "source_count INTEGER DEFAULT 1,"
        "first_learned INTEGER NOT NULL,"
        "last_confirmed INTEGER)";
    sqlite3_exec(db, lessons, NULL, NULL, NULL);
}

static void research_classify_prompt_update(void) {
    hu_research_action_t action = {0};
    hu_error_t err = hu_research_classify_action("update system prompt to be more concise", 39,
                                                 &action);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(action.type, HU_RESEARCH_ACTION_PROMPT_UPDATE);
    HU_ASSERT_TRUE(action.is_safe);
}

static void research_classify_skill_create(void) {
    hu_research_action_t action = {0};
    hu_error_t err = hu_research_classify_action("create new skill for code review", 32, &action);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(action.type, HU_RESEARCH_ACTION_SKILL_CREATE);
    HU_ASSERT_FALSE(action.is_safe);
}

static void research_classify_knowledge(void) {
    hu_research_action_t action = {0};
    hu_error_t err = hu_research_classify_action("learn about topic X from research", 34, &action);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(action.type, HU_RESEARCH_ACTION_KNOWLEDGE_ADD);
    HU_ASSERT_TRUE(action.is_safe);
}

static void research_classify_config(void) {
    hu_research_action_t action = {0};
    hu_error_t err = hu_research_classify_action("change config setting for timeout", 34, &action);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(action.type, HU_RESEARCH_ACTION_CONFIG_SUGGEST);
    HU_ASSERT_FALSE(action.is_safe);
}

static void research_execute_safe_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_tables(db);

    hu_research_action_t action = {0};
    hu_research_classify_action("learn about topic X", 18, &action);
    HU_ASSERT_TRUE(action.is_safe);

    hu_error_t err = hu_research_execute_safe(&alloc, db, &action);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(action.executed);
    HU_ASSERT_TRUE(action.executed_at > 0);

    mem.vtable->deinit(mem.ctx);
}

static void research_dedup_detects_duplicate(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_tables(db);

    hu_error_t err = hu_findings_store(&alloc, db, "source_a", "Finding one", "HIGH", "HIGH", "act");
    HU_ASSERT_EQ(err, HU_OK);

    bool is_dup = false;
    err = hu_research_dedup_finding(&alloc, db, "source_a", 8, "Finding one", 11, &is_dup);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(is_dup);

    mem.vtable->deinit(mem.ctx);
}

static void research_dedup_new_finding(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_tables(db);

    bool is_dup = false;
    hu_error_t err = hu_research_dedup_finding(&alloc, db, "source_b", 8, "New finding", 11, &is_dup);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(is_dup);

    mem.vtable->deinit(mem.ctx);
}

static void research_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    ensure_tables(db);

    hu_research_action_t action = {0};
    hu_error_t err = hu_research_classify_action(NULL, 5, &action);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_research_classify_action("test", 4, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_research_execute_safe(&alloc, db, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    bool is_dup = false;
    err = hu_research_dedup_finding(&alloc, db, "s", 1, NULL, 5, &is_dup);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_research_dedup_finding(&alloc, db, "s", 1, "find", 4, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

void run_research_executor_tests(void) {
    HU_TEST_SUITE("research_executor");
    HU_RUN_TEST(research_classify_prompt_update);
    HU_RUN_TEST(research_classify_skill_create);
    HU_RUN_TEST(research_classify_knowledge);
    HU_RUN_TEST(research_classify_config);
    HU_RUN_TEST(research_execute_safe_succeeds);
    HU_RUN_TEST(research_dedup_detects_duplicate);
    HU_RUN_TEST(research_dedup_new_finding);
    HU_RUN_TEST(research_null_args_returns_error);
}

#else

void run_research_executor_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
