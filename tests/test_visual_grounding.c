#include "human/core/allocator.h"
#include "human/tools/visual_grounding.h"
#include "test_framework.h"

static void visual_grounding_mock_coordinates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    double x = 0, y = 0;
    hu_error_t err = hu_visual_ground_action(&alloc, NULL, NULL, 0, "screenshot.png", 14,
                                             "click the submit button", 23, &x, &y, NULL, NULL);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FLOAT_EQ(x, 100.0, 0.001);
    HU_ASSERT_FLOAT_EQ(y, 200.0, 0.001);
}

static void visual_grounding_mock_returns_selector_when_requested(void) {
    hu_allocator_t alloc = hu_system_allocator();
    double x = 0, y = 0;
    char *sel = NULL;
    size_t sel_len = 0;
    hu_error_t err =
        hu_visual_ground_action(&alloc, NULL, NULL, 0, "screenshot.png", 14, "click submit", 12, &x,
                                &y, &sel, &sel_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(sel);
    HU_ASSERT_STR_EQ(sel, "#mock-grounded-button");
    HU_ASSERT_EQ(sel_len, strlen(sel));
    alloc.free(alloc.ctx, sel, sel_len + 1);
}

void run_visual_grounding_tests(void) {
    HU_TEST_SUITE("VisualGrounding");
    HU_RUN_TEST(visual_grounding_mock_coordinates);
    HU_RUN_TEST(visual_grounding_mock_returns_selector_when_requested);
}
