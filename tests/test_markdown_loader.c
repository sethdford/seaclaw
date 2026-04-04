#include "test_framework.h"

static void markdown_loader_placeholder(void) { HU_ASSERT(1); }

void run_markdown_loader_tests(void) {
    HU_TEST_SUITE("markdown_loader");
    HU_RUN_TEST(markdown_loader_placeholder);
}
