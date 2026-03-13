/*
 * Tests for src/tools/peripheral_ctrl.c — hardware peripheral control tool.
 * Uses HU_IS_TEST mock paths (read returns "0", write/status/capabilities/flash mock).
 */

#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/peripheral.h"
#include "human/tool.h"
#include "human/tools/peripheral_ctrl.h"
#include "test_framework.h"
#include <string.h>

/* Dummy peripheral so execute passes the !c->peripheral check and reaches HU_IS_TEST mock. */
static hu_peripheral_t s_mock_peripheral = {.ctx = NULL, .vtable = NULL};

static void test_peripheral_ctrl_create_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_peripheral_ctrl_tool_create(NULL, NULL, &tool), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_peripheral_ctrl_tool_create(&alloc, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_peripheral_ctrl_create_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_error_t err = hu_peripheral_ctrl_tool_create(&alloc, NULL, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_NOT_NULL(tool.ctx);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, NULL, &tool);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "peripheral_ctrl");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_description(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, NULL, &tool);
    const char *desc = tool.vtable->description(tool.ctx);
    HU_ASSERT_NOT_NULL(desc);
    HU_ASSERT_TRUE(strlen(desc) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_params_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, NULL, &tool);
    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    HU_ASSERT_NOT_NULL(strstr(params, "action"));
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_null_ctx(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, NULL, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "read", 4));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(NULL, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_FALSE(out.success);
    if (out.output_owned && out.output)
        hu_tool_result_free(&alloc, &out);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_missing_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(out.success);
    HU_ASSERT_NOT_NULL(out.error_msg);
    HU_ASSERT_TRUE(out.error_msg_len > 0);
    HU_ASSERT_TRUE(strstr(out.error_msg, "missing action") != NULL);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_read(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "read", 4));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out.success);
    HU_ASSERT_NOT_NULL(out.output);
    HU_ASSERT_EQ(out.output_len, 1u);
    HU_ASSERT_EQ(out.output[0], '0');
    hu_tool_result_free(&alloc, &out);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_write(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "write", 5));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out.success);
    HU_ASSERT_NOT_NULL(out.output);
    HU_ASSERT_STR_EQ(out.output, "ok");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_status(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "status", 6));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out.success);
    HU_ASSERT_STR_EQ(out.output, "healthy");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_capabilities(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "capabilities", 12));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out.success);
    HU_ASSERT_NOT_NULL(out.output);
    HU_ASSERT_NOT_NULL(strstr(out.output, "board_name"));
    hu_tool_result_free(&alloc, &out);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_flash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "flash", 5));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out.success);
    HU_ASSERT_STR_EQ(out.output, "flashed");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_execute_unknown_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, &s_mock_peripheral, &tool);
    hu_json_value_t *args = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(args);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "foo", 3));

    hu_tool_result_t out = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &out);
    hu_json_free(&alloc, args);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(out.success);
    HU_ASSERT_NOT_NULL(strstr(out.error_msg, "unknown action"));
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_peripheral_ctrl_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_peripheral_ctrl_tool_create(&alloc, NULL, &tool);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

void run_peripheral_ctrl_tests(void) {
    HU_TEST_SUITE("peripheral_ctrl");
    HU_RUN_TEST(test_peripheral_ctrl_create_null_args);
    HU_RUN_TEST(test_peripheral_ctrl_create_ok);
    HU_RUN_TEST(test_peripheral_ctrl_name);
    HU_RUN_TEST(test_peripheral_ctrl_description);
    HU_RUN_TEST(test_peripheral_ctrl_params_json);
    HU_RUN_TEST(test_peripheral_ctrl_execute_null_ctx);
    HU_RUN_TEST(test_peripheral_ctrl_execute_missing_action);
    HU_RUN_TEST(test_peripheral_ctrl_execute_read);
    HU_RUN_TEST(test_peripheral_ctrl_execute_write);
    HU_RUN_TEST(test_peripheral_ctrl_execute_status);
    HU_RUN_TEST(test_peripheral_ctrl_execute_capabilities);
    HU_RUN_TEST(test_peripheral_ctrl_execute_flash);
    HU_RUN_TEST(test_peripheral_ctrl_execute_unknown_action);
    HU_RUN_TEST(test_peripheral_ctrl_deinit);
}
