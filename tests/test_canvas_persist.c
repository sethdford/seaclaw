#include "test_framework.h"

static void canvas_persist_placeholder(void) { HU_ASSERT(1); }

void run_canvas_persist_tests(void) {
    HU_TEST_SUITE("CanvasPersist");
    HU_RUN_TEST(canvas_persist_placeholder);
}
