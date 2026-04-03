#include "human/core/allocator.h"
#include "human/tools/save_for_later.h"
#include "test_framework.h"
#include <string.h>

static void save_for_later_create_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_error_t err = hu_save_for_later_create(&alloc, NULL, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_NOT_NULL(tool.ctx);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT_STR_EQ(name, "save_for_later");

    const char *desc = tool.vtable->description(tool.ctx);
    HU_ASSERT_NOT_NULL(desc);
    HU_ASSERT_TRUE(strlen(desc) > 0);

    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    HU_ASSERT_TRUE(strstr(params, "url") != NULL);

    tool.vtable->deinit(tool.ctx, &alloc);
}

static void save_for_later_execute_null_ctx_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_result_t result = {0};
    hu_error_t err = hu_save_for_later_create(&alloc, NULL, &(hu_tool_t){0});
    (void)err;

    hu_tool_t tool = {0};
    hu_save_for_later_create(&alloc, NULL, &tool);

    hu_error_t exec_err = tool.vtable->execute(tool.ctx, &alloc, NULL, &result);
    HU_ASSERT_EQ(exec_err, HU_ERR_INVALID_ARGUMENT);

    tool.vtable->deinit(tool.ctx, &alloc);
}

static void save_for_later_name_matches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_save_for_later_create(&alloc, NULL, &tool), HU_OK);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "save_for_later");
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void save_for_later_params_has_required_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_save_for_later_create(&alloc, NULL, &tool), HU_OK);
    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    HU_ASSERT_TRUE(strstr(params, "url") != NULL);
    HU_ASSERT_TRUE(strstr(params, "summary") != NULL || strstr(params, "topic") != NULL);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void save_for_later_create_with_null_memory_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_save_for_later_create(&alloc, NULL, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    tool.vtable->deinit(tool.ctx, &alloc);
}

int run_save_for_later_tests(void) {
    HU_TEST_SUITE("save_for_later");
    HU_RUN_TEST(save_for_later_create_ok);
    HU_RUN_TEST(save_for_later_execute_null_ctx_fails);
    HU_RUN_TEST(save_for_later_name_matches);
    HU_RUN_TEST(save_for_later_params_has_required_fields);
    HU_RUN_TEST(save_for_later_create_with_null_memory_ok);
    return hu__failed;
}
