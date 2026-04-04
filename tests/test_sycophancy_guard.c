#include "test_framework.h"

static void sycophancy_guard_placeholder(void) { HU_ASSERT(1); }

void run_sycophancy_guard_tests(void) {
    HU_TEST_SUITE("sycophancy_guard");
    HU_RUN_TEST(sycophancy_guard_placeholder);
}
