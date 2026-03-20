#include "test_framework.h"
#include "human/core/json.h"
#include "human/tools/computer_use.h"
#include <string.h>

static void test_cu_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_computer_use_create(&alloc, NULL, &tool), HU_OK);
    HU_ASSERT(tool.vtable != NULL);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "computer_use");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_cu_execute_screenshot_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_computer_use_create(&alloc, NULL, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *shot = "{\"action\":\"screenshot\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, shot, strlen(shot), &args), HU_OK);
    HU_ASSERT_NOT_NULL(args);

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "base64") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_cu_execute_click_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_computer_use_create(&alloc, NULL, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *clk = "{\"action\":\"click\",\"x\":10,\"y\":20}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, clk, strlen(clk), &args), HU_OK);

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_STR_EQ(result.output, "{\"success\":true}");

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_cu_execute_unknown_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_computer_use_create(&alloc, NULL, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *bad = "{\"action\":\"nope\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, bad, strlen(bad), &args), HU_OK);

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

void run_computer_use_tests(void) {
    HU_TEST_SUITE("Computer Use Tool");
    HU_RUN_TEST(test_cu_create);
    HU_RUN_TEST(test_cu_execute_screenshot_mock);
    HU_RUN_TEST(test_cu_execute_click_mock);
    HU_RUN_TEST(test_cu_execute_unknown_action);
}
