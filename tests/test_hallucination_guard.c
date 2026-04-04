#include "test_framework.h"

static void hallucination_guard_placeholder(void) { HU_ASSERT(1); }

void run_hallucination_guard_tests(void) {
    HU_TEST_SUITE("hallucination_guard");
    HU_RUN_TEST(hallucination_guard_placeholder);
}
