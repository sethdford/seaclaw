#include "test_framework.h"

static void humor_fw_placeholder(void) { HU_ASSERT(1); }

void run_humor_fw_tests(void) {
    HU_TEST_SUITE("humor_fw");
    HU_RUN_TEST(humor_fw_placeholder);
}
