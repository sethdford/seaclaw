#include "test_framework.h"

static void fact_extract_placeholder(void) { HU_ASSERT(1); }

void run_fact_extract_tests(void) {
    HU_TEST_SUITE("fact_extract");
    HU_RUN_TEST(fact_extract_placeholder);
}
