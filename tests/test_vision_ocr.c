#include "test_framework.h"

static void vision_ocr_placeholder(void) { HU_ASSERT(1); }

void run_vision_ocr_tests(void) {
    HU_TEST_SUITE("vision_ocr");
    HU_RUN_TEST(vision_ocr_placeholder);
}
