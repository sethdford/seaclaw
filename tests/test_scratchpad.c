#include "human/agent/scratchpad.h"
#include "test_framework.h"
#include <string.h>

static void *test_alloc(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
static void test_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}
static hu_allocator_t test_allocator = {.alloc = test_alloc, .free = test_free, .ctx = NULL};

static void test_scratchpad_init(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 4096);
    HU_ASSERT_EQ(sp.entry_count, 0);
    HU_ASSERT_EQ(sp.total_bytes, 0);
    HU_ASSERT_EQ(sp.max_bytes, 4096);
}

static void test_scratchpad_set_get(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    hu_error_t err = hu_scratchpad_set(&sp, &test_allocator, "key1", 4, "value1", 6);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sp.entry_count, 1);
    HU_ASSERT_EQ(sp.total_bytes, 6);

    const char *val = NULL;
    size_t val_len = 0;
    err = hu_scratchpad_get(&sp, "key1", 4, &val, &val_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(val);
    HU_ASSERT_EQ(val_len, 6);
    HU_ASSERT_TRUE(memcmp(val, "value1", 6) == 0);

    hu_scratchpad_deinit(&sp, &test_allocator);
}

static void test_scratchpad_overwrite(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    hu_scratchpad_set(&sp, &test_allocator, "k", 1, "old", 3);
    HU_ASSERT_EQ(sp.total_bytes, 3);

    hu_scratchpad_set(&sp, &test_allocator, "k", 1, "new_value", 9);
    HU_ASSERT_EQ(sp.entry_count, 1);
    HU_ASSERT_EQ(sp.total_bytes, 9);

    const char *val = NULL;
    hu_scratchpad_get(&sp, "k", 1, &val, NULL);
    HU_ASSERT_NOT_NULL(val);
    HU_ASSERT_TRUE(memcmp(val, "new_value", 9) == 0);

    hu_scratchpad_deinit(&sp, &test_allocator);
}

static void test_scratchpad_delete(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    hu_scratchpad_set(&sp, &test_allocator, "a", 1, "1", 1);
    hu_scratchpad_set(&sp, &test_allocator, "b", 1, "22", 2);
    HU_ASSERT_EQ(sp.entry_count, 2);

    hu_error_t err = hu_scratchpad_delete(&sp, &test_allocator, "a", 1);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sp.entry_count, 1);
    HU_ASSERT_EQ(sp.total_bytes, 2);
    HU_ASSERT_FALSE(hu_scratchpad_has(&sp, "a", 1));
    HU_ASSERT_TRUE(hu_scratchpad_has(&sp, "b", 1));

    hu_scratchpad_deinit(&sp, &test_allocator);
}

static void test_scratchpad_delete_not_found(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    hu_error_t err = hu_scratchpad_delete(&sp, &test_allocator, "nope", 4);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
}

static void test_scratchpad_has(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    HU_ASSERT_FALSE(hu_scratchpad_has(&sp, "x", 1));
    hu_scratchpad_set(&sp, &test_allocator, "x", 1, "y", 1);
    HU_ASSERT_TRUE(hu_scratchpad_has(&sp, "x", 1));

    hu_scratchpad_deinit(&sp, &test_allocator);
}

static void test_scratchpad_to_json(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    hu_scratchpad_set(&sp, &test_allocator, "key", 3, "val", 3);
    char buf[256];
    size_t len = hu_scratchpad_to_json(&sp, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(buf, "\"key\"");
    HU_ASSERT_STR_CONTAINS(buf, "\"val\"");

    hu_scratchpad_deinit(&sp, &test_allocator);
}

static void test_scratchpad_max_bytes(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 10);

    hu_error_t err = hu_scratchpad_set(&sp, &test_allocator, "a", 1, "12345", 5);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_scratchpad_set(&sp, &test_allocator, "b", 1, "1234567890", 10);
    HU_ASSERT_EQ(err, HU_ERR_OUT_OF_MEMORY);
    HU_ASSERT_EQ(sp.entry_count, 1);

    hu_scratchpad_deinit(&sp, &test_allocator);
}

static void test_scratchpad_clear(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    hu_scratchpad_set(&sp, &test_allocator, "a", 1, "1", 1);
    hu_scratchpad_set(&sp, &test_allocator, "b", 1, "2", 1);
    HU_ASSERT_EQ(sp.entry_count, 2);

    hu_scratchpad_clear(&sp, &test_allocator);
    HU_ASSERT_EQ(sp.entry_count, 0);
    HU_ASSERT_EQ(sp.total_bytes, 0);
}

static void test_scratchpad_get_not_found(void) {
    hu_scratchpad_t sp;
    hu_scratchpad_init(&sp, 0);

    const char *val = NULL;
    hu_error_t err = hu_scratchpad_get(&sp, "missing", 7, &val, NULL);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
}

static void test_scratchpad_null_args(void) {
    hu_scratchpad_init(NULL, 0);
    HU_ASSERT_EQ(hu_scratchpad_set(NULL, NULL, "a", 1, "b", 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_scratchpad_get(NULL, "a", 1, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_scratchpad_delete(NULL, NULL, "a", 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_FALSE(hu_scratchpad_has(NULL, "a", 1));
}

void run_scratchpad_tests(void) {
    HU_TEST_SUITE("Scratchpad");
    HU_RUN_TEST(test_scratchpad_init);
    HU_RUN_TEST(test_scratchpad_set_get);
    HU_RUN_TEST(test_scratchpad_overwrite);
    HU_RUN_TEST(test_scratchpad_delete);
    HU_RUN_TEST(test_scratchpad_delete_not_found);
    HU_RUN_TEST(test_scratchpad_has);
    HU_RUN_TEST(test_scratchpad_to_json);
    HU_RUN_TEST(test_scratchpad_max_bytes);
    HU_RUN_TEST(test_scratchpad_clear);
    HU_RUN_TEST(test_scratchpad_get_not_found);
    HU_RUN_TEST(test_scratchpad_null_args);
}
