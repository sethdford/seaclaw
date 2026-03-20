#include "human/core/json.h"
#include "human/tools/browser_use.h"
#include "test_framework.h"
#include <string.h>

static void browser_use_create_registers_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "browser_use");
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_navigate_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"action\":\"navigate\",\"url\":\"https://example.com/path\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_target_prefers_css_selector(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *js = "{\"action\":\"click\",\"target\":\"submit control\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, js, strlen(js), &args), HU_OK);

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "click_via") != NULL);
    HU_ASSERT(strstr(result.output, "selector") != NULL);
    HU_ASSERT(strstr(result.output, "#mock-grounded-button") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

void run_browser_use_tests(void) {
    HU_TEST_SUITE("BrowserUse");
    HU_RUN_TEST(browser_use_create_registers_name);
    HU_RUN_TEST(browser_use_navigate_mock_ok);
    HU_RUN_TEST(browser_use_target_prefers_css_selector);
}
