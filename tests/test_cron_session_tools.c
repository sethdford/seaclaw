#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/cron.h"
#include "human/tool.h"
#include "human/tools/cron_session_tools.h"
#include <stdlib.h>
#include <string.h>

static hu_allocator_t g_alloc;
static hu_cron_scheduler_t *g_sched;

static void setup(void) {
    g_alloc = hu_system_allocator();
    g_sched = hu_cron_create(&g_alloc, 100, true);
    HU_ASSERT_NOT_NULL(g_sched);
}

static void teardown(void) {
    if (g_sched) {
        hu_cron_destroy(g_sched, &g_alloc);
        g_sched = NULL;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * cron_create tool tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_cron_create_tool_create(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));

    hu_error_t err = hu_cron_create_session_tool_create(&g_alloc, g_sched, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_NOT_NULL(tool.vtable);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);

    teardown();
}

static void test_cron_create_tool_name(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_create_session_tool_create(&g_alloc, g_sched, &tool);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "cron_create");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_cron_create_tool_missing_expr(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_create_session_tool_create(&g_alloc, g_sched, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);
    hu_json_object_set(&g_alloc, args, "prompt",
                       hu_json_string_new(&g_alloc, "test prompt", 11));

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(!result.success);

    hu_json_free(&g_alloc, args);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_cron_create_tool_execute_valid(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_create_session_tool_create(&g_alloc, g_sched, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);
    hu_json_object_set(&g_alloc, args, "cron_expr",
                       hu_json_string_new(&g_alloc, "*/5 * * * *", 11));
    hu_json_object_set(&g_alloc, args, "prompt",
                       hu_json_string_new(&g_alloc, "test prompt", 11));

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_STR_CONTAINS(result.output, "id");

    if (result.output_owned && result.output)
        g_alloc.free(g_alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&g_alloc, args);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

/* ──────────────────────────────────────────────────────────────────────────
 * cron_delete tool tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_cron_delete_tool_create(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));

    hu_error_t err = hu_cron_delete_session_tool_create(&g_alloc, g_sched, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_NOT_NULL(tool.vtable);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);

    teardown();
}

static void test_cron_delete_tool_name(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_delete_session_tool_create(&g_alloc, g_sched, &tool);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "cron_delete");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_cron_delete_tool_execute_valid(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_delete_session_tool_create(&g_alloc, g_sched, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);
    hu_json_object_set(&g_alloc, args, "id", hu_json_number_new(&g_alloc, 42.0));

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "deleted");

    if (result.output_owned && result.output)
        g_alloc.free(g_alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&g_alloc, args);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

/* ──────────────────────────────────────────────────────────────────────────
 * cron_list tool tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_cron_list_tool_create(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));

    hu_error_t err = hu_cron_list_session_tool_create(&g_alloc, g_sched, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_NOT_NULL(tool.vtable);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);

    teardown();
}

static void test_cron_list_tool_name(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_list_session_tool_create(&g_alloc, g_sched, &tool);

    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "cron_list");

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

static void test_cron_list_tool_execute_empty(void) {
    setup();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_cron_list_session_tool_create(&g_alloc, g_sched, &tool);

    hu_json_value_t *args = hu_json_object_new(&g_alloc);

    hu_tool_result_t result;
    memset(&result, 0, sizeof(result));

    hu_error_t err = tool.vtable->execute(tool.ctx, &g_alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "jobs");

    if (result.output_owned && result.output)
        g_alloc.free(g_alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&g_alloc, args);

    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &g_alloc);
    teardown();
}

void run_cron_session_tools_tests(void) {
    HU_TEST_SUITE("Cron Session Tools");

    HU_RUN_TEST(test_cron_create_tool_create);
    HU_RUN_TEST(test_cron_create_tool_name);
    HU_RUN_TEST(test_cron_create_tool_missing_expr);
    HU_RUN_TEST(test_cron_create_tool_execute_valid);

    HU_RUN_TEST(test_cron_delete_tool_create);
    HU_RUN_TEST(test_cron_delete_tool_name);
    HU_RUN_TEST(test_cron_delete_tool_execute_valid);

    HU_RUN_TEST(test_cron_list_tool_create);
    HU_RUN_TEST(test_cron_list_tool_name);
    HU_RUN_TEST(test_cron_list_tool_execute_empty);
}
