#ifdef HU_ENABLE_SQLITE

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/intelligence/cycle.h"
#include "human/memory.h"
#include "test_framework.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>

static void ensure_opinions_table(sqlite3 *db) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS opinions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "topic TEXT NOT NULL,"
        "position TEXT NOT NULL,"
        "confidence REAL NOT NULL,"
        "first_expressed INTEGER NOT NULL,"
        "last_expressed INTEGER NOT NULL,"
        "superseded_by INTEGER)";
    sqlite3_exec(db, sql, NULL, NULL, NULL);
}

static void insert_finding(sqlite3 *db, const char *source, const char *finding,
                           const char *relevance, const char *priority,
                           const char *action, int64_t created_at) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO research_findings "
                      "(source, finding, relevance, priority, suggested_action, "
                      "status, created_at) VALUES (?, ?, ?, ?, ?, 'pending', ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, finding, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, relevance, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, priority, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, action, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, created_at);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void insert_feed_item(sqlite3 *db, const char *source, const char *content_type,
                             const char *content, const char *url, int64_t ingested_at) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO feed_items "
                      "(source, contact_id, content_type, content, url, ingested_at) "
                      "VALUES (?, 'system', ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, url, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, ingested_at);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int count_rows(sqlite3 *db, const char *table) {
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table);
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

/* --- NULL arg tests --- */

static void cycle_null_alloc_returns_invalid(void) {
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    hu_intelligence_cycle_result_t r = {0};
    HU_ASSERT_EQ(hu_intelligence_run_cycle(NULL, db, &r), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

static void cycle_null_db_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_intelligence_cycle_result_t r = {0};
    HU_ASSERT_EQ(hu_intelligence_run_cycle(&alloc, NULL, &r), HU_ERR_INVALID_ARGUMENT);
}

static void cycle_null_result_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    HU_ASSERT_EQ(hu_intelligence_run_cycle(&alloc, db, NULL), HU_ERR_INVALID_ARGUMENT);
    sqlite3_close(db);
}

/* --- Empty DB test --- */

static void cycle_empty_db_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_TRUE(err == HU_OK || err == HU_ERR_IO);
    HU_ASSERT_EQ(r.findings_actioned, 0u);
    HU_ASSERT_EQ(r.events_recorded, 0u);
    HU_ASSERT_EQ(r.lessons_extracted, 0u);
    HU_ASSERT_EQ(r.values_learned, 0u);
    HU_ASSERT_EQ(r.causal_recorded, 0u);

    mem.vtable->deinit(mem.ctx);
}

/* --- Full cycle with findings and feed items --- */

static void cycle_with_findings_and_feeds_processes_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "arxiv", "Transformer scaling improves accuracy",
                   "high", "HIGH",
                   "Investigate scaling laws further with repeated analysis",
                   now - 60);
    insert_finding(db, "arxiv", "Attention mechanisms reduce latency",
                   "medium", "MEDIUM",
                   "Optimize attention module performance improvements",
                   now - 30);
    insert_finding(db, "github", "New library improves inference speed",
                   "high", "HIGH",
                   "Benchmark inference engine against baseline performance",
                   now - 10);

    insert_feed_item(db, "rss_ai_news", "article",
                     "GPT-5 announced with breakthrough capabilities",
                     "https://example.com/gpt5", now - 100);
    insert_feed_item(db, "twitter_ai", "post",
                     "Major advancement in reinforcement learning published today",
                     "https://example.com/rl", now - 50);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);

    HU_ASSERT_TRUE(r.findings_actioned >= 2);
    HU_ASSERT_TRUE(r.events_recorded >= 1);
    HU_ASSERT_TRUE(r.causal_recorded >= 1);

    sqlite3_stmt *chk = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM research_findings WHERE status = 'actioned'",
        -1, &chk, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(chk);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    int actioned = sqlite3_column_int(chk, 0);
    sqlite3_finalize(chk);
    HU_ASSERT_TRUE(actioned >= 2);

    HU_ASSERT_TRUE(count_rows(db, "current_events") >= 1);

    mem.vtable->deinit(mem.ctx);
}

/* --- Findings only (no feeds) --- */

static void cycle_findings_only_no_events(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "paper", "Novel architecture outperforms baseline",
                   "high", "HIGH",
                   "Replicate experiment to verify claims",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r.findings_actioned >= 1);
    HU_ASSERT_EQ(r.events_recorded, 0u);

    mem.vtable->deinit(mem.ctx);
}

/* --- Feeds only (no findings) --- */

static void cycle_feeds_only_no_findings(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_feed_item(db, "rss_tech", "article",
                     "Cloud computing infrastructure shows growth trajectory",
                     "https://example.com/cloud", now - 30);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_TRUE(err == HU_OK || err == HU_ERR_IO);
    HU_ASSERT_EQ(r.findings_actioned, 0u);
    HU_ASSERT_TRUE(r.events_recorded >= 1);

    mem.vtable->deinit(mem.ctx);
}

/* --- Lessons extraction from recurring words --- */

static void cycle_recurring_words_extract_lessons(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "src1", "Finding about scaling",
                   "high", "HIGH",
                   "Investigate scaling approach thoroughly",
                   now - 120);
    insert_finding(db, "src2", "Finding about optimization",
                   "high", "HIGH",
                   "Investigate scaling methods completely",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r.findings_actioned >= 2);
    HU_ASSERT_TRUE(r.lessons_extracted >= 1);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT lesson FROM general_lessons WHERE lesson LIKE '%recurring theme%'",
        -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(stmt);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    const char *lesson = (const char *)sqlite3_column_text(stmt, 0);
    HU_ASSERT_NOT_NULL(lesson);
    HU_ASSERT_TRUE(strlen(lesson) > 0);
    sqlite3_finalize(stmt);

    mem.vtable->deinit(mem.ctx);
}

/* --- Value learning from HIGH findings --- */

static void cycle_high_findings_learn_values(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "paper", "Alignment techniques improve safety",
                   "critical", "HIGH",
                   "Prioritize safety-first development approach",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r.findings_actioned >= 1);
    HU_ASSERT_TRUE(r.values_learned >= 1);

    mem.vtable->deinit(mem.ctx);
}

/* --- Opinions populated from HIGH findings --- */

static void cycle_high_findings_populate_opinions(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "research", "Sparse models are more efficient",
                   "high", "HIGH",
                   "Adopt sparse model architectures",
                   now - 60);
    insert_finding(db, "benchmark", "Dense models achieve higher accuracy",
                   "high", "HIGH",
                   "Maintain dense model baselines",
                   now - 30);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);

    int opinion_count = count_rows(db, "opinions");
    HU_ASSERT_TRUE(opinion_count >= 2);

    mem.vtable->deinit(mem.ctx);
}

/* --- Skills created from recurring sources --- */

static void cycle_recurring_source_creates_skills(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "arxiv_ml", "New training method improves convergence",
                   "high", "HIGH",
                   "Monitor arxiv_ml for training improvements",
                   now - 120);
    insert_finding(db, "arxiv_ml", "Better regularization reduces overfitting",
                   "medium", "MEDIUM",
                   "Apply regularization techniques broadly",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r.skills_updated >= 1);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT name FROM skills WHERE name LIKE 'monitor_%'",
        -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(stmt);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    const char *name = (const char *)sqlite3_column_text(stmt, 0);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_TRUE(strstr(name, "monitor_") == name);
    sqlite3_finalize(stmt);

    mem.vtable->deinit(mem.ctx);
}

/* --- Cognitive load logged --- */

static void cycle_logs_cognitive_load(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "test", "Test finding for cognitive load",
                   "high", "HIGH", "Action needed immediately",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);

    int load_count = count_rows(db, "cognitive_load_log");
    HU_ASSERT_TRUE(load_count >= 1);

    mem.vtable->deinit(mem.ctx);
}

/* --- Growth milestone recorded when findings actioned --- */

static void cycle_records_growth_milestone(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "src", "Important finding for milestones",
                   "high", "HIGH", "Track this milestone carefully",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r.findings_actioned >= 1);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT after_state FROM growth_milestones WHERE topic = 'research_cycle'",
        -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(stmt);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    const char *after = (const char *)sqlite3_column_text(stmt, 0);
    HU_ASSERT_NOT_NULL(after);
    HU_ASSERT_TRUE(strstr(after, "actioned") != NULL);
    sqlite3_finalize(stmt);

    mem.vtable->deinit(mem.ctx);
}

/* --- Behavioral feedback seeded on successful cycle --- */

static void cycle_seeds_behavioral_feedback(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "src", "Finding triggers feedback",
                   "high", "HIGH", "Process this finding urgently",
                   now - 60);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    HU_ASSERT_EQ(err, HU_OK);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT signal, context FROM behavioral_feedback "
        "WHERE behavior_type = 'research_cycle'",
        -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(stmt);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    HU_ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "positive");
    const char *ctx = (const char *)sqlite3_column_text(stmt, 1);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(strstr(ctx, "Cycle processed") != NULL);
    sqlite3_finalize(stmt);

    mem.vtable->deinit(mem.ctx);
}

/* --- Result zeroed on entry --- */

static void cycle_result_zeroed_on_entry(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    hu_intelligence_cycle_result_t r;
    memset(&r, 0xFF, sizeof(r));

    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    (void)err;

    HU_ASSERT_EQ(r.findings_actioned, 0u);
    HU_ASSERT_EQ(r.events_recorded, 0u);
    HU_ASSERT_EQ(r.lessons_extracted, 0u);
    HU_ASSERT_EQ(r.values_learned, 0u);
    HU_ASSERT_EQ(r.causal_recorded, 0u);

    mem.vtable->deinit(mem.ctx);
}

/* --- LOW priority findings are not actioned --- */

static void cycle_low_priority_findings_skipped(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.vtable);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);
    ensure_opinions_table(db);

    int64_t now = (int64_t)time(NULL);

    insert_finding(db, "src", "Low priority finding",
                   "low", "LOW", "Maybe look at this later",
                   now - 60);

    int pre_low_count = 0;
    {
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM research_findings WHERE priority = 'LOW' AND status = 'pending'",
            -1, &s, NULL);
        if (sqlite3_step(s) == SQLITE_ROW)
            pre_low_count = sqlite3_column_int(s, 0);
        sqlite3_finalize(s);
    }
    HU_ASSERT_TRUE(pre_low_count >= 1);

    hu_intelligence_cycle_result_t r = {0};
    hu_error_t err = hu_intelligence_run_cycle(&alloc, db, &r);
    (void)err;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM research_findings WHERE priority = 'LOW' AND status = 'pending'",
        -1, &stmt, NULL);
    HU_ASSERT_EQ(rc, SQLITE_OK);
    rc = sqlite3_step(stmt);
    HU_ASSERT_EQ(rc, SQLITE_ROW);
    int post_low_count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    HU_ASSERT_EQ(post_low_count, pre_low_count);

    mem.vtable->deinit(mem.ctx);
}

void run_intelligence_cycle_tests(void) {
    HU_TEST_SUITE("intelligence_cycle");
    HU_RUN_TEST(cycle_null_alloc_returns_invalid);
    HU_RUN_TEST(cycle_null_db_returns_invalid);
    HU_RUN_TEST(cycle_null_result_returns_invalid);
    HU_RUN_TEST(cycle_empty_db_no_crash);
    HU_RUN_TEST(cycle_result_zeroed_on_entry);
    HU_RUN_TEST(cycle_with_findings_and_feeds_processes_all);
    HU_RUN_TEST(cycle_findings_only_no_events);
    HU_RUN_TEST(cycle_feeds_only_no_findings);
    HU_RUN_TEST(cycle_recurring_words_extract_lessons);
    HU_RUN_TEST(cycle_high_findings_learn_values);
    HU_RUN_TEST(cycle_high_findings_populate_opinions);
    HU_RUN_TEST(cycle_recurring_source_creates_skills);
    HU_RUN_TEST(cycle_logs_cognitive_load);
    HU_RUN_TEST(cycle_records_growth_milestone);
    HU_RUN_TEST(cycle_seeds_behavioral_feedback);
    HU_RUN_TEST(cycle_low_priority_findings_skipped);
}

#else

void run_intelligence_cycle_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
