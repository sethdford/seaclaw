#include "test_framework.h"

static void trust_calibration_placeholder(void) { HU_ASSERT(1); }

void run_trust_calibration_tests(void) {
    HU_TEST_SUITE("trust_calibration");
    HU_RUN_TEST(trust_calibration_placeholder);
}
