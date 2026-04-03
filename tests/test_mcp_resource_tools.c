#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/tool.h"
#include "human/tools/mcp_resource_tools.h"
#include <stdlib.h>
#include <string.h>

static hu_allocator_t g_alloc;

static void setup(void) { g_alloc = hu_system_allocator(); }

static void teardown(void) {
    /* No cleanup needed for system allocator */
}

/* ──────────────────────────────────────────────────────────────────────────
 * list_mcp_resources tool tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_list_resources_tool_create(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));

    hu_error_t err = hu_mcp_resource_list_tool_create(&g_alloc, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_NOT_NULL(tool.vtable->execute);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);

    teardown();
}

static void test_list_resources_tool_name(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_list_tool_create(&g_alloc, &tool);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "list_mcp_resources");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_list_resources_tool_description(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_list_tool_create(&g_alloc, &tool);

    const char *desc = tool.vtable->description(tool.ctx);
    HU_ASSERT_NOT_NULL(desc);
    HU_ASSERT_STR_CONTAINS(desc, "resource");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_list_resources_tool_parameters(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_list_tool_create(&g_alloc, &tool);

    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    HU_ASSERT_STR_CONTAINS(params, "server");
    HU_ASSERT_STR_CONTAINS(params, "type");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_list_resources_tool_execute_valid(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_list_tool_create(&g_alloc, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);
    hu_json_object_set(&g_alloc, args, "server",
                       hu_json_string_new(&g_alloc, "filesystem", 10));

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_STR_CONTAINS(result.output, "resources");

    if (result.output_owned && result.output)
        g_alloc.free(g_alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&g_alloc, args);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

/* ──────────────────────────────────────────────────────────────────────────
 * read_mcp_resource tool tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_read_resource_tool_create(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));

    hu_error_t err = hu_mcp_resource_read_tool_create(&g_alloc, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_NOT_NULL(tool.vtable->execute);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);

    teardown();
}

static void test_read_resource_tool_name(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_read_tool_create(&g_alloc, &tool);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "read_mcp_resource");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_read_resource_tool_missing_server(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_read_tool_create(&g_alloc, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);
    hu_json_object_set(&g_alloc, args, "uri",
                       hu_json_string_new(&g_alloc, "file:///etc/passwd", 18));

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(!result.success);
    HU_ASSERT_NOT_NULL(result.error_msg);

    hu_json_free(&g_alloc, args);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_read_resource_tool_execute_valid(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_mcp_resource_read_tool_create(&g_alloc, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);
    hu_json_object_set(&g_alloc, args, "server",
                       hu_json_string_new(&g_alloc, "filesystem", 10));
    hu_json_object_set(&g_alloc, args, "uri",
                       hu_json_string_new(&g_alloc, "file:///etc/passwd", 18));

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_STR_CONTAINS(result.output, "uri");

    if (result.output_owned && result.output)
        g_alloc.free(g_alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&g_alloc, args);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

void run_mcp_resource_tools_tests(void) {
    HU_TEST_SUITE("MCP Resource Tools");

    HU_RUN_TEST(test_list_resources_tool_create);
    HU_RUN_TEST(test_list_resources_tool_name);
    HU_RUN_TEST(test_list_resources_tool_description);
    HU_RUN_TEST(test_list_resources_tool_parameters);
    HU_RUN_TEST(test_list_resources_tool_execute_valid);

    HU_RUN_TEST(test_read_resource_tool_create);
    HU_RUN_TEST(test_read_resource_tool_name);
    HU_RUN_TEST(test_read_resource_tool_missing_server);
    HU_RUN_TEST(test_read_resource_tool_execute_valid);
}
