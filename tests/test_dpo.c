#include "test_framework.h"
#include "human/ml/dpo.h"
#include <string.h>

static void dpo_create_and_init_tables(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 100, &col), HU_OK);
    HU_ASSERT_EQ((int)col.pair_count, 0);
    HU_ASSERT_EQ((int)col.max_pairs, 100);
    HU_ASSERT_EQ(hu_dpo_init_tables(&col), HU_OK);
    hu_dpo_collector_deinit(&col);
}

static void dpo_record_pair_stores_correctly(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    hu_preference_pair_t pair;
    memset(&pair, 0, sizeof(pair));
    memcpy(pair.prompt, "test prompt", 11);
    pair.prompt_len = 11;
    memcpy(pair.chosen, "good answer", 11);
    pair.chosen_len = 11;
    memcpy(pair.rejected, "bad answer", 10);
    pair.rejected_len = 10;
    pair.margin = 0.9;
    HU_ASSERT_EQ(hu_dpo_record_pair(&col, &pair), HU_OK);
    size_t count = 0;
    HU_ASSERT_EQ(hu_dpo_pair_count(&col, &count), HU_OK);
    HU_ASSERT_EQ((int)count, 1);
    hu_dpo_collector_deinit(&col);
}

static void dpo_record_from_feedback_positive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    HU_ASSERT_EQ(hu_dpo_record_from_feedback(&col, "prompt", 6, "response", 8, true), HU_OK);
    size_t count = 0;
    hu_dpo_pair_count(&col, &count);
    HU_ASSERT_EQ((int)count, 1);
    hu_dpo_collector_deinit(&col);
}

static void dpo_record_from_feedback_negative(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    HU_ASSERT_EQ(hu_dpo_record_from_feedback(&col, "prompt", 6, "bad resp", 8, false), HU_OK);
    size_t count = 0;
    hu_dpo_pair_count(&col, &count);
    HU_ASSERT_EQ((int)count, 1);
    hu_dpo_collector_deinit(&col);
}

static void dpo_record_from_retry(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    HU_ASSERT_EQ(hu_dpo_record_from_retry(&col, "prompt", 6, "rejected", 8, "chosen", 6), HU_OK);
    size_t count = 0;
    hu_dpo_pair_count(&col, &count);
    HU_ASSERT_EQ((int)count, 1);
    hu_dpo_collector_deinit(&col);
}

static void dpo_export_jsonl_format(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    hu_dpo_record_from_feedback(&col, "p", 1, "r", 1, true);
    size_t exported = 0;
    HU_ASSERT_EQ(hu_dpo_export_jsonl(&col, "test.jsonl", 10, &exported), HU_OK);
    HU_ASSERT_EQ((int)exported, 1);
    hu_dpo_collector_deinit(&col);
}

static void dpo_pair_count_accurate(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    hu_dpo_record_from_feedback(&col, "p1", 2, "r1", 2, true);
    hu_dpo_record_from_feedback(&col, "p2", 2, "r2", 2, false);
    hu_dpo_record_from_retry(&col, "p3", 2, "rej", 3, "cho", 3);
    size_t count = 0;
    hu_dpo_pair_count(&col, &count);
    HU_ASSERT_EQ((int)count, 3);
    hu_dpo_collector_deinit(&col);
}

static void dpo_clear_removes_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    hu_dpo_record_from_feedback(&col, "p", 1, "r", 1, true);
    HU_ASSERT_EQ(hu_dpo_clear(&col), HU_OK);
    size_t count = 999;
    hu_dpo_pair_count(&col, &count);
    HU_ASSERT_EQ((int)count, 0);
    hu_dpo_collector_deinit(&col);
}

#ifdef HU_ENABLE_SQLITE
static void dpo_max_pairs_ring_buffer(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, db, 3, &col), HU_OK);
    HU_ASSERT_EQ(hu_dpo_init_tables(&col), HU_OK);
    for (int i = 0; i < 5; i++)
        hu_dpo_record_from_feedback(&col, "p", 1, "r", 1, true);
    /* DB should have at most 3 rows due to ring buffer eviction */
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM dpo_pairs", -1, &stmt, NULL);
    sqlite3_step(stmt);
    int db_count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    HU_ASSERT(db_count <= 3);
    hu_dpo_collector_deinit(&col);
    sqlite3_close(db);
}
#endif

static void dpo_null_collector_returns_error(void) {
    HU_ASSERT_EQ(hu_dpo_record_pair(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    size_t out = 0;
    HU_ASSERT_EQ(hu_dpo_pair_count(NULL, &out), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_dpo_clear(NULL), HU_ERR_INVALID_ARGUMENT);
}

static void dpo_empty_export_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    size_t exported = 999;
    HU_ASSERT_EQ(hu_dpo_export_jsonl(&col, "out.jsonl", 9, &exported), HU_OK);
    HU_ASSERT_EQ((int)exported, 0);
    hu_dpo_collector_deinit(&col);
}

static void dpo_margin_reflects_confidence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_dpo_collector_t col;
    HU_ASSERT_EQ(hu_dpo_collector_create(&alloc, NULL, 0, &col), HU_OK);
    /* record_from_feedback uses margin=0.7, record_from_retry uses margin=0.8 */
    HU_ASSERT_EQ(hu_dpo_record_from_feedback(&col, "p", 1, "r", 1, true), HU_OK);
    HU_ASSERT_EQ(hu_dpo_record_from_retry(&col, "p", 1, "rej", 3, "cho", 3), HU_OK);
    size_t count = 0;
    hu_dpo_pair_count(&col, &count);
    HU_ASSERT_EQ((int)count, 2);
    hu_dpo_collector_deinit(&col);
}

void run_dpo_tests(void) {
    HU_TEST_SUITE("DPO Preference");
    HU_RUN_TEST(dpo_create_and_init_tables);
    HU_RUN_TEST(dpo_record_pair_stores_correctly);
    HU_RUN_TEST(dpo_record_from_feedback_positive);
    HU_RUN_TEST(dpo_record_from_feedback_negative);
    HU_RUN_TEST(dpo_record_from_retry);
    HU_RUN_TEST(dpo_export_jsonl_format);
    HU_RUN_TEST(dpo_pair_count_accurate);
    HU_RUN_TEST(dpo_clear_removes_all);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(dpo_max_pairs_ring_buffer);
#endif
    HU_RUN_TEST(dpo_null_collector_returns_error);
    HU_RUN_TEST(dpo_empty_export_succeeds);
    HU_RUN_TEST(dpo_margin_reflects_confidence);
}
