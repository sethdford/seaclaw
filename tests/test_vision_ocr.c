#include "human/core/allocator.h"
#include "human/tools/vision_ocr.h"
#include "test_framework.h"
#include <string.h>

static void test_vision_ocr_mock_results(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    hu_error_t err = hu_vision_ocr_recognize(&alloc, "/tmp/test.png", &results, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 3);
    HU_ASSERT_NOT_NULL(results);
    HU_ASSERT_STR_EQ(results[0].text, "File");
    HU_ASSERT_STR_EQ(results[1].text, "Edit");
    HU_ASSERT_STR_EQ(results[2].text, "Save");

    hu_ocr_results_free(&alloc, results, count);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_vision_ocr_find_target(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    hu_vision_ocr_recognize(&alloc, "/tmp/test.png", &results, &count);

    double x = 0, y = 0;
    hu_error_t err = hu_vision_ocr_find_target(&alloc, results, count, "save", 4, &x, &y);
    HU_ASSERT_EQ(err, HU_OK);
    /* Save is at x=100, y=200, w=60, h=20 -> center = (130, 210) */
    HU_ASSERT_TRUE(x > 120 && x < 140);
    HU_ASSERT_TRUE(y > 200 && y < 220);

    hu_error_t err2 = hu_vision_ocr_find_target(&alloc, results, count, "nonexist", 8, &x, &y);
    HU_ASSERT_EQ(err2, HU_ERR_NOT_FOUND);

    hu_ocr_results_free(&alloc, results, count);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_vision_ocr_null_args(void) {
    hu_error_t err = hu_vision_ocr_recognize(NULL, NULL, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);

    err = hu_vision_ocr_find_target(NULL, NULL, 0, NULL, 0, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

void run_vision_ocr_tests(void) {
    HU_TEST_SUITE("vision_ocr");
    HU_RUN_TEST(test_vision_ocr_mock_results);
    HU_RUN_TEST(test_vision_ocr_find_target);
    HU_RUN_TEST(test_vision_ocr_null_args);
}
