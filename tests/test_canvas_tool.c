#include "test_framework.h"

static void canvas_tool_placeholder(void) { HU_ASSERT(1); }

void run_canvas_tool_tests(void) {
    HU_TEST_SUITE("CanvasTool");
    HU_RUN_TEST(canvas_tool_placeholder);
}
