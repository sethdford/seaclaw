#include "human/core/json.h"
#include "human/tools/image_gen.h"
#include "test_framework.h"
#include <string.h>

static void image_gen_create_registers_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_image_gen_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "image_generate");
}

static void image_gen_execute_returns_mock_url(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_image_gen_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json_str = "{\"prompt\":\"a sunset over mountains\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json_str, strlen(json_str), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(hu__strcasestr(result.output, "mock") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void image_gen_url_into_buffer_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char url[512];
    HU_ASSERT_EQ(hu_image_gen_url_into_buffer(&alloc, "a red balloon", 13, url, sizeof(url)), HU_OK);
    HU_ASSERT(hu__strcasestr(url, "mock") != NULL);
}

static void image_gen_missing_prompt_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_image_gen_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json_str = "{\"size\":\"1024x1024\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json_str, strlen(json_str), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void image_gen_download_returns_mock_path(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char path[512];
    HU_ASSERT_EQ(hu_image_gen_download(&alloc, "a cozy cat", 10, path, sizeof(path)), HU_OK);
    HU_ASSERT(strstr(path, "/tmp/") != NULL || strstr(path, "hu_test") != NULL);
}

static void image_gen_download_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char path[512];
    HU_ASSERT_EQ(hu_image_gen_download(NULL, "x", 1, path, sizeof(path)), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_image_gen_download(&alloc, NULL, 0, path, sizeof(path)), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_image_gen_download(&alloc, "x", 1, path, 2), HU_ERR_INVALID_ARGUMENT);
}

void run_image_gen_tests(void) {
    HU_TEST_SUITE("ImageGen");
    HU_RUN_TEST(image_gen_create_registers_name);
    HU_RUN_TEST(image_gen_execute_returns_mock_url);
    HU_RUN_TEST(image_gen_url_into_buffer_mock);
    HU_RUN_TEST(image_gen_missing_prompt_fails);
    HU_RUN_TEST(image_gen_download_returns_mock_path);
    HU_RUN_TEST(image_gen_download_null_args);
}
