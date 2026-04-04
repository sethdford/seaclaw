#include "human/core/allocator.h"
#include "human/tool.h"
#include "human/tools/vision_ocr.h"
#include "test_framework.h"
#include <string.h>

static void ocr_recognize_test_returns_three_results(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_vision_ocr_recognize(&alloc, "/mock/image.png", &results, &count), HU_OK);
    HU_ASSERT_EQ((long long)count, 3LL);
    HU_ASSERT_NOT_NULL(results);
    hu_ocr_results_free(&alloc, results, count);
}

static void ocr_recognize_first_is_file(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_vision_ocr_recognize(&alloc, "/mock/image.png", &results, &count), HU_OK);
    HU_ASSERT_NOT_NULL(results);
    HU_ASSERT_NOT_NULL(results[0].text);
    HU_ASSERT_STR_EQ(results[0].text, "File");
    hu_ocr_results_free(&alloc, results, count);
}

static void ocr_find_target_file_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_vision_ocr_recognize(&alloc, "/mock/image.png", &results, &count), HU_OK);
    double x = 0.0;
    double y = 0.0;
    HU_ASSERT_EQ(hu_vision_ocr_find_target(&alloc, results, count, "File", 4U, &x, &y), HU_OK);
    HU_ASSERT_TRUE(x > 0.0);
    HU_ASSERT_TRUE(y > 0.0);
    hu_ocr_results_free(&alloc, results, count);
}

static void ocr_find_target_missing_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_vision_ocr_recognize(&alloc, "/mock/image.png", &results, &count), HU_OK);
    double x = 0.0;
    double y = 0.0;
    HU_ASSERT_EQ(hu_vision_ocr_find_target(&alloc, results, count, "nonexistent", 11U, &x, &y),
                 HU_ERR_NOT_FOUND);
    hu_ocr_results_free(&alloc, results, count);
}

static void ocr_results_free_null_is_safe(void) {
    hu_ocr_results_free(NULL, NULL, 0U);
}

static void ocr_tool_create_and_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_vision_ocr_tool_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_NOT_NULL(tool.vtable->execute);
    HU_ASSERT_NOT_NULL(tool.vtable->name);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "vision_ocr");

    hu_tool_result_t res = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, NULL, &res), HU_OK);
    HU_ASSERT_TRUE(res.success);
    hu_tool_result_free(&alloc, &res);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

void run_vision_ocr_tests(void) {
    HU_TEST_SUITE("vision_ocr");
    HU_RUN_TEST(ocr_recognize_test_returns_three_results);
    HU_RUN_TEST(ocr_recognize_first_is_file);
    HU_RUN_TEST(ocr_find_target_file_succeeds);
    HU_RUN_TEST(ocr_find_target_missing_returns_error);
    HU_RUN_TEST(ocr_results_free_null_is_safe);
    HU_RUN_TEST(ocr_tool_create_and_deinit);
}
