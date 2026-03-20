#include "human/core/allocator.h"
#include "human/tools/visual_grounding.h"
#include "test_framework.h"

static void visual_grounding_mock_coordinates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    double x = 0, y = 0;
    hu_error_t err = hu_visual_ground_action(&alloc, NULL, NULL, 0, "screenshot.png", 14,
                                             "click the submit button", 23, &x, &y, NULL, NULL);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FLOAT_EQ(x, 512.0, 0.001);
    HU_ASSERT_FLOAT_EQ(y, 384.0, 0.001);
}

void run_visual_grounding_tests(void) {
    HU_TEST_SUITE("VisualGrounding");
    HU_RUN_TEST(visual_grounding_mock_coordinates);
}
