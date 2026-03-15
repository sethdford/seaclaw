#include "test_framework.h"
#include "human/memory/forgetting.h"

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

void run_forgetting_tests(void) {
    HU_TEST_SUITE("Memory Forgetting");
    HU_RUN_TEST(test_forgetting_null);
    HU_RUN_TEST(test_forgetting_bad_rate);
    HU_RUN_TEST(test_forgetting_boost_null);
}
