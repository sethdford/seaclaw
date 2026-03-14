#include "test_framework.h"
#include "human/agent/tool_call_parser.h"
#include "human/core/allocator.h"
#include <string.h>

static void test_parse_single_tool_call(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text =
        "Let me check that for you.\n"
        "<tool_call>{\"name\": \"weather\", \"arguments\": {\"city\": \"London\"}}</tool_call>";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT(calls != NULL);
    HU_ASSERT(strcmp(calls[0].name, "weather") == 0);
    HU_ASSERT(calls[0].arguments != NULL);
    HU_ASSERT(strstr(calls[0].arguments, "London") != NULL);
    HU_ASSERT(calls[0].id != NULL);
    hu_text_tool_calls_free(&alloc, calls, count);
}

static void test_parse_multiple_tool_calls(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text =
        "<tool_call>{\"name\": \"read_file\", \"arguments\": {\"path\": \"/tmp/a.txt\"}}</tool_call>\n"
        "Now the second one:\n"
        "<tool_call>{\"name\": \"write_file\", \"arguments\": {\"path\": \"/tmp/b.txt\", "
        "\"content\": \"hello\"}}</tool_call>";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2);
    HU_ASSERT(strcmp(calls[0].name, "read_file") == 0);
    HU_ASSERT(strcmp(calls[1].name, "write_file") == 0);
    hu_text_tool_calls_free(&alloc, calls, count);
}

static void test_parse_no_tool_calls(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "I don't need any tools for that. The answer is 42.";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0);
    HU_ASSERT(calls == NULL);
}

static void test_parse_malformed_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "<tool_call>not valid json</tool_call>";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0);
}

static void test_parse_missing_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "<tool_call>{\"arguments\": {\"x\": 1}}</tool_call>";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0);
}

static void test_parse_no_arguments(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "<tool_call>{\"name\": \"list_files\"}</tool_call>";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT(strcmp(calls[0].name, "list_files") == 0);
    HU_ASSERT(strcmp(calls[0].arguments, "{}") == 0);
    hu_text_tool_calls_free(&alloc, calls, count);
}

static void test_parse_empty_input(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, "", 0, &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0);
}

static void test_parse_null_args(void) {
    hu_error_t err = hu_text_tool_calls_parse(NULL, "x", 1, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_strip_tool_calls(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text =
        "Hello! <tool_call>{\"name\": \"test\", \"arguments\": {}}</tool_call> Done.";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_text_tool_calls_strip(&alloc, text, strlen(text), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(out != NULL);
    HU_ASSERT(strstr(out, "Hello!") != NULL);
    HU_ASSERT(strstr(out, "Done.") != NULL);
    HU_ASSERT(strstr(out, "tool_call") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_strip_only_tool_calls(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "<tool_call>{\"name\": \"test\", \"arguments\": {}}</tool_call>";
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_text_tool_calls_strip(&alloc, text, strlen(text), &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(out == NULL);
    HU_ASSERT_EQ(out_len, 0);
}

static void test_parse_whitespace_around_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "<tool_call>\n  {\"name\": \"shell\", \"arguments\": {\"cmd\": \"ls\"}}  \n</tool_call>";
    hu_tool_call_t *calls = NULL;
    size_t count = 0;
    hu_error_t err = hu_text_tool_calls_parse(&alloc, text, strlen(text), &calls, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT(strcmp(calls[0].name, "shell") == 0);
    hu_text_tool_calls_free(&alloc, calls, count);
}

void run_tool_call_parser_tests(void) {
    HU_TEST_SUITE("tool_call_parser");
    HU_RUN_TEST(test_parse_single_tool_call);
    HU_RUN_TEST(test_parse_multiple_tool_calls);
    HU_RUN_TEST(test_parse_no_tool_calls);
    HU_RUN_TEST(test_parse_malformed_json);
    HU_RUN_TEST(test_parse_missing_name);
    HU_RUN_TEST(test_parse_no_arguments);
    HU_RUN_TEST(test_parse_empty_input);
    HU_RUN_TEST(test_parse_null_args);
    HU_RUN_TEST(test_strip_tool_calls);
    HU_RUN_TEST(test_strip_only_tool_calls);
    HU_RUN_TEST(test_parse_whitespace_around_json);
}
