#include "test_framework.h"
#include "human/memory/forgetting.h"

#include <string.h>

static void test_forgetting_null(void) {
    hu_forgetting_stats_t stats;
    HU_ASSERT_EQ(hu_memory_decay(NULL, NULL, 0.1, &stats), HU_ERR_INVALID_ARGUMENT);
}

static void test_forgetting_bad_rate(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_forgetting_stats_t stats;
    HU_ASSERT_EQ(hu_memory_decay(&alloc, NULL, -0.5, &stats), HU_ERR_INVALID_ARGUMENT);
}

static void test_forgetting_boost_null(void) {
    HU_ASSERT_EQ(hu_memory_boost(NULL, NULL, "mem-1", 1.0), HU_ERR_INVALID_ARGUMENT);
}

static void test_forgetting_boost_null_memory_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_memory_boost(&alloc, NULL, "mem-1", 1.0), HU_ERR_INVALID_ARGUMENT);
}

static void test_forgetting_decay_null_memory_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_forgetting_stats_t stats;
    HU_ASSERT_EQ(hu_memory_decay(&alloc, NULL, 0.1, &stats), HU_ERR_INVALID_ARGUMENT);
}

static void test_forgetting_boost_empty_key_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    HU_ASSERT_EQ(hu_memory_boost(&alloc, &memory, "", 1.0), HU_ERR_INVALID_ARGUMENT);
}

static void test_forgetting_boost_absent_key_returns_not_found(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t memory;
    memset(&memory, 0, sizeof(memory));
    HU_ASSERT_EQ(hu_memory_boost(&alloc, &memory, "__hu_test_absent_memory__", 1.0), HU_ERR_NOT_FOUND);
}

void run_forgetting_tests(void) {
    HU_TEST_SUITE("Memory Forgetting");
    HU_RUN_TEST(test_forgetting_null);
    HU_RUN_TEST(test_forgetting_bad_rate);
    HU_RUN_TEST(test_forgetting_boost_null);
    HU_RUN_TEST(test_forgetting_decay_null_memory_returns_error);
    HU_RUN_TEST(test_forgetting_boost_null_memory_returns_error);
    HU_RUN_TEST(test_forgetting_boost_empty_key_returns_error);
    HU_RUN_TEST(test_forgetting_boost_absent_key_returns_not_found);
}

