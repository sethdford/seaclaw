#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/tools/canvas.h"
#include "test_framework.h"
#include <string.h>

static void test_canvas_tool_create_execute_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_canvas_tool_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "canvas");

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "create", 6));
    hu_json_object_set(&alloc, args, "title", hu_json_string_new(&alloc, "UI", 2));
    hu_json_object_set(&alloc, args, "format", hu_json_string_new(&alloc, "html", 4));

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "canvas_id") != NULL);

    if (result.output_owned && result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_update_requires_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_canvas_tool_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "update", 6));
    hu_json_object_set(&alloc, args, "content", hu_json_string_new(&alloc, "<p>x</p>", 7));

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT_FALSE(result.success);

    hu_json_free(&alloc, args);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

void run_canvas_tool_tests(void) {
    HU_TEST_SUITE("canvas_tool");
    HU_RUN_TEST(test_canvas_tool_create_execute_create);
    HU_RUN_TEST(test_canvas_tool_update_requires_id);
}
