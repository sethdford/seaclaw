#include "human/agent/idempotency.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

/* Test: create and destroy registry */
static void test_idempotency_create_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(reg);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: record and check hit */
static void test_idempotency_record_and_hit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *args_json = "{\"param\": \"value\"}";
    const char *result_json = "{\"output\": \"success\"}";

    /* Record the result */
    err = hu_idempotency_record(reg, &alloc, tool_name, args_json, result_json, false);
    HU_ASSERT_EQ(err, HU_OK);

    /* Check if we can retrieve it */
    hu_idempotency_entry_t entry;
    bool hit = hu_idempotency_check(reg, tool_name, args_json, &entry);
    HU_ASSERT(hit);
    HU_ASSERT_STR_EQ(entry.result_json, result_json);
    HU_ASSERT(!entry.is_error);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: check miss with different args */
static void test_idempotency_check_miss(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *args_json1 = "{\"param\": \"value1\"}";
    const char *args_json2 = "{\"param\": \"value2\"}";
    const char *result_json = "{\"output\": \"success\"}";

    /* Record with args1 */
    err = hu_idempotency_record(reg, &alloc, tool_name, args_json1, result_json, false);
    HU_ASSERT_EQ(err, HU_OK);

    /* Check with args2 should miss */
    hu_idempotency_entry_t entry;
    bool hit = hu_idempotency_check(reg, tool_name, args_json2, &entry);
    HU_ASSERT(!hit);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: key generation is deterministic */
static void test_idempotency_key_generation_deterministic(void) {
    hu_allocator_t alloc = hu_system_allocator();

    const char *tool_name = "my_tool";
    const char *args_json = "{\"a\": 1, \"b\": 2}";

    char *key1 = NULL;
    size_t key1_len = 0;
    hu_error_t err1 = hu_idempotency_key_generate(&alloc, tool_name, args_json, &key1, &key1_len);
    HU_ASSERT_EQ(err1, HU_OK);

    char *key2 = NULL;
    size_t key2_len = 0;
    hu_error_t err2 = hu_idempotency_key_generate(&alloc, tool_name, args_json, &key2, &key2_len);
    HU_ASSERT_EQ(err2, HU_OK);

    /* Keys should be identical */
    HU_ASSERT_EQ(key1_len, key2_len);
    HU_ASSERT_STR_EQ(key1, key2);

    alloc.free(alloc.ctx, key1, key1_len + 1);
    alloc.free(alloc.ctx, key2, key2_len + 1);
}

/* Test: key generation differs for different args */
static void test_idempotency_key_generation_different(void) {
    hu_allocator_t alloc = hu_system_allocator();

    const char *tool_name = "my_tool";
    const char *args_json1 = "{\"a\": 1}";
    const char *args_json2 = "{\"a\": 2}";

    char *key1 = NULL;
    size_t key1_len = 0;
    hu_error_t err1 = hu_idempotency_key_generate(&alloc, tool_name, args_json1, &key1, &key1_len);
    HU_ASSERT_EQ(err1, HU_OK);

    char *key2 = NULL;
    size_t key2_len = 0;
    hu_error_t err2 = hu_idempotency_key_generate(&alloc, tool_name, args_json2, &key2, &key2_len);
    HU_ASSERT_EQ(err2, HU_OK);

    /* Keys should differ */
    HU_ASSERT_NEQ(strcmp(key1, key2), 0);

    alloc.free(alloc.ctx, key1, key1_len + 1);
    alloc.free(alloc.ctx, key2, key2_len + 1);
}

/* Test: LRU eviction at capacity */
static void test_idempotency_lru_eviction(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *result_json = "{\"output\": \"success\"}";

    /* Fill registry to near capacity */
    char args_buf[256];
    for (int i = 0; i < 4000; i++) {
        snprintf(args_buf, sizeof(args_buf), "{\"id\": %d}", i);
        err = hu_idempotency_record(reg, &alloc, tool_name, args_buf, result_json, false);
        HU_ASSERT_EQ(err, HU_OK);
    }

    hu_idempotency_stats_t stats;
    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.entry_count, 4000);

    /* Add more entries, should trigger eviction */
    for (int i = 4000; i < 4100; i++) {
        snprintf(args_buf, sizeof(args_buf), "{\"id\": %d}", i);
        err = hu_idempotency_record(reg, &alloc, tool_name, args_buf, result_json, false);
        HU_ASSERT_EQ(err, HU_OK);
    }

    hu_idempotency_stats(reg, &stats);
    /* Should be at or near max capacity */
    HU_ASSERT_GT(stats.entry_count, 4000);
    HU_ASSERT_LT(stats.entry_count, 4200);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: clear empties registry */
static void test_idempotency_clear(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *result_json = "{\"output\": \"success\"}";

    /* Record some entries */
    for (int i = 0; i < 10; i++) {
        char args_buf[256];
        snprintf(args_buf, sizeof(args_buf), "{\"id\": %d}", i);
        err = hu_idempotency_record(reg, &alloc, tool_name, args_buf, result_json, false);
        HU_ASSERT_EQ(err, HU_OK);
    }

    hu_idempotency_stats_t stats;
    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.entry_count, 10);

    /* Clear registry */
    hu_idempotency_clear(reg, &alloc);

    hu_idempotency_stats(reg, &stats);
    HU_ASSERT_EQ(stats.entry_count, 0);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: record error result */
static void test_idempotency_error_result(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *args_json = "{\"param\": \"value\"}";
    const char *error_json = "{\"error\": \"tool failed\"}";

    /* Record an error result */
    err = hu_idempotency_record(reg, &alloc, tool_name, args_json, error_json, true);
    HU_ASSERT_EQ(err, HU_OK);

    /* Check if we can retrieve it with is_error flag */
    hu_idempotency_entry_t entry;
    bool hit = hu_idempotency_check(reg, tool_name, args_json, &entry);
    HU_ASSERT(hit);
    HU_ASSERT_STR_EQ(entry.result_json, error_json);
    HU_ASSERT(entry.is_error);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: memory cleanup with tracking allocator */
static void test_idempotency_memory_cleanup(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_idempotency_registry_t *reg = NULL;
    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *result_json = "{\"output\": \"success\"}";

    /* Record several entries */
    for (int i = 0; i < 5; i++) {
        char args_buf[256];
        snprintf(args_buf, sizeof(args_buf), "{\"id\": %d}", i);
        err = hu_idempotency_record(reg, &alloc, tool_name, args_buf, result_json, false);
        HU_ASSERT_EQ(err, HU_OK);
    }

    /* Destroy and check for leaks */
    hu_idempotency_destroy(reg, &alloc);

    size_t leaks = hu_tracking_allocator_leaks(ta);
    HU_ASSERT_EQ(leaks, 0);

    hu_tracking_allocator_destroy(ta);
}

/* Test: multiple tools with same args */
static void test_idempotency_multiple_tools(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *args_json = "{\"param\": \"value\"}";
    const char *result1 = "{\"output\": \"from_tool1\"}";
    const char *result2 = "{\"output\": \"from_tool2\"}";

    /* Record same args with different tools */
    err = hu_idempotency_record(reg, &alloc, "tool1", args_json, result1, false);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_idempotency_record(reg, &alloc, "tool2", args_json, result2, false);
    HU_ASSERT_EQ(err, HU_OK);

    /* Both should be retrievable */
    hu_idempotency_entry_t entry1, entry2;

    bool hit1 = hu_idempotency_check(reg, "tool1", args_json, &entry1);
    HU_ASSERT(hit1);
    HU_ASSERT_STR_EQ(entry1.result_json, result1);

    bool hit2 = hu_idempotency_check(reg, "tool2", args_json, &entry2);
    HU_ASSERT(hit2);
    HU_ASSERT_STR_EQ(entry2.result_json, result2);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: stats tracking */
static void test_idempotency_stats(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *args_json1 = "{\"id\": 1}";
    const char *args_json2 = "{\"id\": 2}";
    const char *result_json = "{\"output\": \"success\"}";

    /* Record entries */
    err = hu_idempotency_record(reg, &alloc, tool_name, args_json1, result_json, false);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_idempotency_record(reg, &alloc, tool_name, args_json2, result_json, false);
    HU_ASSERT_EQ(err, HU_OK);

    /* Check hits and misses */
    hu_idempotency_entry_t entry;
    bool hit1 = hu_idempotency_check(reg, tool_name, args_json1, &entry);
    HU_ASSERT(hit1);

    bool hit2 = hu_idempotency_check(reg, tool_name, args_json2, &entry);
    HU_ASSERT(hit2);

    /* Miss with non-existent args */
    bool miss = hu_idempotency_check(reg, tool_name, "{\"id\": 99}", &entry);
    HU_ASSERT(!miss);

    hu_idempotency_stats_t stats;
    hu_idempotency_stats(reg, &stats);

    HU_ASSERT_EQ(stats.entry_count, 2);
    HU_ASSERT_EQ(stats.max_entries, HU_IDEMPOTENCY_MAX_ENTRIES);
    HU_ASSERT_EQ(stats.total_hits, 2);
    HU_ASSERT_EQ(stats.total_misses, 1);

    hu_idempotency_destroy(reg, &alloc);
}

/* Test: update existing entry */
static void test_idempotency_update_existing(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_idempotency_registry_t *reg = NULL;

    hu_error_t err = hu_idempotency_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);

    const char *tool_name = "test_tool";
    const char *args_json = "{\"param\": \"value\"}";
    const char *result1 = "{\"output\": \"first\"}";
    const char *result2 = "{\"output\": \"second\"}";

    /* Record initial result */
    err = hu_idempotency_record(reg, &alloc, tool_name, args_json, result1, false);
    HU_ASSERT_EQ(err, HU_OK);

    hu_idempotency_entry_t entry;
    bool hit = hu_idempotency_check(reg, tool_name, args_json, &entry);
    HU_ASSERT(hit);
    HU_ASSERT_STR_EQ(entry.result_json, result1);

    /* Update with new result */
    err = hu_idempotency_record(reg, &alloc, tool_name, args_json, result2, false);
    HU_ASSERT_EQ(err, HU_OK);

    /* Should retrieve updated result */
    hit = hu_idempotency_check(reg, tool_name, args_json, &entry);
    HU_ASSERT(hit);
    HU_ASSERT_STR_EQ(entry.result_json, result2);

    hu_idempotency_destroy(reg, &alloc);
}

void run_idempotency_tests(void) {
    HU_TEST_SUITE("idempotency");
    HU_RUN_TEST(test_idempotency_create_destroy);
    HU_RUN_TEST(test_idempotency_record_and_hit);
    HU_RUN_TEST(test_idempotency_check_miss);
    HU_RUN_TEST(test_idempotency_key_generation_deterministic);
    HU_RUN_TEST(test_idempotency_key_generation_different);
    HU_RUN_TEST(test_idempotency_lru_eviction);
    HU_RUN_TEST(test_idempotency_clear);
    HU_RUN_TEST(test_idempotency_error_result);
    HU_RUN_TEST(test_idempotency_memory_cleanup);
    HU_RUN_TEST(test_idempotency_multiple_tools);
    HU_RUN_TEST(test_idempotency_stats);
    HU_RUN_TEST(test_idempotency_update_existing);
}
