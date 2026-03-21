#include "human/core/json.h"
#include "human/tools/browser_use.h"
#include "test_framework.h"
#include <string.h>

#if defined(HU_IS_TEST) && HU_IS_TEST

/* Keep in sync with HU_BU_MAX_URL in src/tools/browser_use.c */
enum { browser_use_test_max_url_len = 2048U };

static void browser_use_screenshot_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"action\":\"screenshot\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "base64") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_type_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json =
        "{\"action\":\"type\",\"selector\":\"#mock-input\",\"text\":\"hello from test\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_extract_text_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"action\":\"extract_text\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "mock extracted text") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_execute_js_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"action\":\"execute_js\",\"script\":\"return 1\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "mock result") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_navigate_http_allowed_in_test_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"action\":\"navigate\",\"url\":\"http://example.com/path\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "http://example.com/path") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_navigate_empty_url_returns_invalid_argument(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"action\":\"navigate\",\"url\":\"\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

static void browser_use_navigate_url_over_max_len_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_browser_use_create(&alloc, &tool), HU_OK);

    char url[browser_use_test_max_url_len + 2];
    memcpy(url, "https://", 8);
    memset(url + 8, 'a', (size_t)browser_use_test_max_url_len + 1 - 8);
    url[browser_use_test_max_url_len + 1] = '\0';
    HU_ASSERT_EQ(strlen(url), (size_t)browser_use_test_max_url_len + 1);

    char json_buf[sizeof(url) + 64];
    int jn = snprintf(json_buf, sizeof(json_buf),
        "{\"action\":\"navigate\",\"url\":\"%s\"}", url);
    HU_ASSERT_GT(jn, 0);
    HU_ASSERT_GT(sizeof(json_buf), (size_t)jn);

    hu_json_value_t *args = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc, json_buf, (size_t)jn, &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    hu_browser_use_destroy(&alloc, &tool);
}

#endif /* HU_IS_TEST */

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
#if defined(HU_IS_TEST) && HU_IS_TEST
    HU_RUN_TEST(browser_use_screenshot_mock_ok);
    HU_RUN_TEST(browser_use_type_mock_ok);
    HU_RUN_TEST(browser_use_extract_text_mock_ok);
    HU_RUN_TEST(browser_use_execute_js_mock_ok);
    HU_RUN_TEST(browser_use_navigate_http_allowed_in_test_mock);
    HU_RUN_TEST(browser_use_navigate_empty_url_returns_invalid_argument);
    HU_RUN_TEST(browser_use_navigate_url_over_max_len_fails);
#endif
}
