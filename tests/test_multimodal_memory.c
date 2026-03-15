#include "test_framework.h"

#ifdef HU_ENABLE_SQLITE

#include "human/core/allocator.h"
#include "human/memory/multimodal_index.h"
#include <sqlite3.h>
#include <string.h>

static void test_multimodal_memory_init_tables(void) {
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_error_t err = hu_multimodal_memory_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);
    sqlite3_close(db);
}

static void test_multimodal_memory_store_and_search(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_error_t err = hu_multimodal_memory_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    const char *desc = "A photo of a sunset over the ocean";
    err = hu_multimodal_memory_store(&alloc, db, HU_MODALITY_IMAGE, desc, strlen(desc));
    HU_ASSERT_EQ(err, HU_OK);

    hu_multimodal_memory_entry_t results[8];
    size_t count = 0;
    err = hu_multimodal_memory_search(&alloc, db, "sunset", 6, results, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_EQ(results[0].modality, HU_MODALITY_IMAGE);
    HU_ASSERT_STR_EQ(results[0].description, desc);

    sqlite3_close(db);
}

static void test_multimodal_memory_search_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_error_t err = hu_multimodal_memory_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    hu_multimodal_memory_entry_t results[8];
    size_t count = 99;
    err = hu_multimodal_memory_search(&alloc, db, "keyword", 7, results, 8, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0u);

    sqlite3_close(db);
}

static void test_multimodal_memory_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    sqlite3 *db = NULL;
    HU_ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    hu_multimodal_memory_init_tables(db);

    hu_multimodal_memory_entry_t results[8];
    size_t count = 0;

    HU_ASSERT_EQ(hu_multimodal_memory_init_tables(NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_memory_store(NULL, db, HU_MODALITY_IMAGE, "x", 1),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_memory_store(&alloc, NULL, HU_MODALITY_IMAGE, "x", 1),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_memory_store(&alloc, db, HU_MODALITY_IMAGE, NULL, 0),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_memory_search(&alloc, NULL, "q", 1, results, 8, &count),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_memory_search(&alloc, db, "q", 1, NULL, 8, &count),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_memory_search(&alloc, db, "q", 1, results, 8, NULL),
                 HU_ERR_INVALID_ARGUMENT);

    sqlite3_close(db);
}

#endif /* HU_ENABLE_SQLITE */

void run_multimodal_memory_tests(void) {
    HU_TEST_SUITE("MultimodalMemory");
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(test_multimodal_memory_init_tables);
    HU_RUN_TEST(test_multimodal_memory_store_and_search);
    HU_RUN_TEST(test_multimodal_memory_search_empty);
    HU_RUN_TEST(test_multimodal_memory_null_args_returns_error);
#endif
}
