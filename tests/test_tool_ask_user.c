#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/tools/ask_user.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static void test_ask_user_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_tool_ask_user_create(&alloc, NULL);
    HU_ASSERT(tool.ctx != NULL);
    HU_ASSERT(tool.vtable != NULL);
    HU_ASSERT_EQ(strcmp(tool.vtable->name(tool.ctx), "ask_user"), 0);
    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &alloc);
    }
}

static void test_ask_user_name_and_description(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_tool_ask_user_create(&alloc, NULL);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT(strcmp(name, "ask_user") == 0);

    const char *desc = tool.vtable->description(tool.ctx);
    HU_ASSERT(desc != NULL && strlen(desc) > 0);

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &alloc);
    }
}

static void test_ask_user_parameters_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_tool_ask_user_create(&alloc, NULL);

    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT(params != NULL);
    HU_ASSERT(strstr(params, "question") != NULL);

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &alloc);
    }
}

static void test_ask_user_execute_without_question(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_tool_ask_user_create(&alloc, NULL);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_tool_result_t result;

    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT(result.error_msg != NULL);

    hu_json_free(&alloc, args);

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &alloc);
    }
}

static void test_ask_user_execute_with_question(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_tool_ask_user_create(&alloc, NULL);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "question", hu_json_string_new(&alloc, "What is your name?", 18));
    hu_tool_result_t result;

    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT(result.output != NULL);

    hu_json_free(&alloc, args);

    if (result.output_owned && result.output) {
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    }

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &alloc);
    }
}

static void test_ask_user_execute_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_tool_ask_user_create(&alloc, NULL);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "question", hu_json_string_new(&alloc, "Test question?", 14));
    hu_tool_result_t result;

    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_TRUE(result.output_owned);

    hu_json_free(&alloc, args);

    if (result.output_owned && result.output) {
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    }

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &alloc);
    }
}

void run_tool_ask_user_tests(void) {
    HU_TEST_SUITE("Tool ask_user");
    HU_RUN_TEST(test_ask_user_create);
    HU_RUN_TEST(test_ask_user_name_and_description);
    HU_RUN_TEST(test_ask_user_parameters_json);
    HU_RUN_TEST(test_ask_user_execute_without_question);
    HU_RUN_TEST(test_ask_user_execute_with_question);
    HU_RUN_TEST(test_ask_user_execute_test_mode);
}
