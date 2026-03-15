#include "human/core/allocator.h"
#include "human/memory.h"
#include "human/memory/engines.h"
#include "test_framework.h"
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * hu_memory_entry_free_fields
 * ────────────────────────────────────────────────────────────────────────── */

static void test_entry_free_fields_null_alloc_returns_early(void) {
    hu_memory_entry_t e = {0};
    e.id = "x";
    e.id_len = 1;
    hu_memory_entry_free_fields(NULL, &e);
    /* No crash; early return when alloc is NULL */
}

static void test_entry_free_fields_null_entry_returns_early(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_entry_free_fields(&alloc, NULL);
    /* No crash; early return when entry is NULL */
}

static void test_entry_free_fields_both_null_returns_early(void) {
    hu_memory_entry_free_fields(NULL, NULL);
    /* No crash */
}

static void test_entry_free_fields_empty_entry_no_op(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_entry_t e = {0};
    hu_memory_entry_free_fields(&alloc, &e);
    /* No crash; all fields NULL */
}

static void test_entry_free_fields_frees_all_allocated_fields(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *id = alloc.alloc(alloc.ctx, 4);
    char *key = alloc.alloc(alloc.ctx, 5);
    char *content = alloc.alloc(alloc.ctx, 9);
    char *timestamp = alloc.alloc(alloc.ctx, 6);
    char *session_id = alloc.alloc(alloc.ctx, 5);
    char *source = alloc.alloc(alloc.ctx, 8);
    char *custom_name = alloc.alloc(alloc.ctx, 7);

    HU_ASSERT_NOT_NULL(id);
    HU_ASSERT_NOT_NULL(key);
    strcpy(id, "id1");
    strcpy(key, "key1");
    strcpy(content, "content1");
    strcpy(timestamp, "ts123");
    strcpy(session_id, "sess");
    strcpy(source, "source1");
    strcpy(custom_name, "custom");

    hu_memory_entry_t e = {
        .id = id,
        .id_len = 3,
        .key = key,
        .key_len = 4,
        .content = content,
        .content_len = 8,
        .category =
            {
                .tag = HU_MEMORY_CATEGORY_CUSTOM,
                .data = {.custom = {.name = custom_name, .name_len = 6}},
            },
        .timestamp = timestamp,
        .timestamp_len = 5,
        .session_id = session_id,
        .session_id_len = 4,
        .source = source,
        .source_len = 6,
    };

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 7u);
    hu_memory_entry_free_fields(&alloc, &e);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0u);

    hu_tracking_allocator_destroy(ta);
}

static void test_entry_free_fields_key_eq_id_frees_once(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *buf = alloc.alloc(alloc.ctx, 5);
    HU_ASSERT_NOT_NULL(buf);
    strcpy(buf, "key1");

    hu_memory_entry_t e = {
        .id = buf,
        .id_len = 4,
        .key = buf,
        .key_len = 4,
        .content = NULL,
        .content_len = 0,
    };

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 1u);
    hu_memory_entry_free_fields(&alloc, &e);
    /* key == id, so only one free; no double-free */
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0u);

    hu_tracking_allocator_destroy(ta);
}

static void test_entry_free_fields_partial_fields(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *content = alloc.alloc(alloc.ctx, 4);
    HU_ASSERT_NOT_NULL(content);
    strcpy(content, "val");

    hu_memory_entry_t e = {
        .id = NULL,
        .id_len = 0,
        .key = NULL,
        .key_len = 0,
        .content = content,
        .content_len = 3,
    };

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 1u);
    hu_memory_entry_free_fields(&alloc, &e);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0u);

    hu_tracking_allocator_destroy(ta);
}

/* ──────────────────────────────────────────────────────────────────────────
 * hu_memory_store_with_source
 * ────────────────────────────────────────────────────────────────────────── */

static void test_store_with_source_null_mem_returns_invalid(void) {
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};
    hu_error_t err = hu_memory_store_with_source(NULL, "key", 3, "content", 7, &cat, NULL, 0, NULL,
                                                 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_store_with_source_null_vtable_returns_invalid(void) {
    hu_memory_t mem = {.ctx = (void *)1, .vtable = NULL};
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};
    hu_error_t err = hu_memory_store_with_source(&mem, "key", 3, "content", 7, &cat, NULL, 0, NULL,
                                                 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_store_with_source_no_source_falls_back_to_store(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_none_memory_create(&alloc);
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};

    hu_error_t err = hu_memory_store_with_source(&mem, "key", 3, "content", 7, &cat, NULL, 0, NULL,
                                                 0);
    HU_ASSERT_EQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

static void test_store_with_source_source_len_zero_falls_back_to_store(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_none_memory_create(&alloc);
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};

    hu_error_t err = hu_memory_store_with_source(&mem, "key", 3, "content", 7, &cat, NULL, 0,
                                                 "file:///x", 0);
    HU_ASSERT_EQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

static void test_store_with_source_with_source_none_has_no_store_ex_falls_back(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_none_memory_create(&alloc);
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};

    /* none engine has no store_ex; falls back to store */
    hu_error_t err = hu_memory_store_with_source(&mem, "key", 3, "content", 7, &cat, NULL, 0,
                                                 "file:///doc.md", 13);
    HU_ASSERT_EQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

#ifdef HU_ENABLE_SQLITE
static void test_store_with_source_sqlite_uses_store_ex_when_source_provided(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    hu_memory_category_t cat = {.tag = HU_MEMORY_CATEGORY_CORE};

    hu_error_t err = hu_memory_store_with_source(&mem, "key", 3, "content", 7, &cat, NULL, 0,
                                                 "file:///doc.md", 13);
    HU_ASSERT_EQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}
#endif

void run_memory_util_tests(void) {
    HU_TEST_SUITE("MemoryUtil");
    HU_RUN_TEST(test_entry_free_fields_null_alloc_returns_early);
    HU_RUN_TEST(test_entry_free_fields_null_entry_returns_early);
    HU_RUN_TEST(test_entry_free_fields_both_null_returns_early);
    HU_RUN_TEST(test_entry_free_fields_empty_entry_no_op);
    HU_RUN_TEST(test_entry_free_fields_frees_all_allocated_fields);
    HU_RUN_TEST(test_entry_free_fields_key_eq_id_frees_once);
    HU_RUN_TEST(test_entry_free_fields_partial_fields);
    HU_RUN_TEST(test_store_with_source_null_mem_returns_invalid);
    HU_RUN_TEST(test_store_with_source_null_vtable_returns_invalid);
    HU_RUN_TEST(test_store_with_source_no_source_falls_back_to_store);
    HU_RUN_TEST(test_store_with_source_source_len_zero_falls_back_to_store);
    HU_RUN_TEST(test_store_with_source_with_source_none_has_no_store_ex_falls_back);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(test_store_with_source_sqlite_uses_store_ex_when_source_provided);
#endif
}
