#include "test_framework.h"

static void consistency_placeholder(void) { HU_ASSERT(1); }

void run_consistency_tests(void) {
    HU_TEST_SUITE("consistency");
    HU_RUN_TEST(consistency_placeholder);
}
