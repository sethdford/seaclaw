#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/task_manager.h"
#include "human/tools/task_tools.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static hu_allocator_t test_alloc;
static hu_task_manager_t *test_mgr;

static void setup_test_manager(void) {
    hu_task_manager_create(&test_alloc, 100, &test_mgr);
}

static void teardown_test_manager(void) {
    hu_task_manager_destroy(test_mgr, &test_alloc);
    test_mgr = NULL;
}

static void test_task_create_tool_create(void) {
    setup_test_manager();

    hu_tool_t tool = hu_tool_task_create(&test_alloc, test_mgr);
    HU_ASSERT(tool.ctx != NULL);
    HU_ASSERT(tool.vtable != NULL);

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

static void test_task_create_tool_name(void) {
    setup_test_manager();

    hu_tool_t tool = hu_tool_task_create(&test_alloc, test_mgr);
    const char *name = tool.vtable->name(tool.ctx);
    HU_ASSERT(strcmp(name, "task_create") == 0);

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

static void test_task_create_execute(void) {
    setup_test_manager();

    hu_tool_t tool = hu_tool_task_create(&test_alloc, test_mgr);

    hu_json_value_t *args = hu_json_object_new(&test_alloc);
    hu_json_object_set(&test_alloc, args, "subject", hu_json_string_new(&test_alloc, "Test Task", 9));
    hu_json_object_set(&test_alloc, args, "description", hu_json_string_new(&test_alloc, "This is a test task", 19));

    hu_tool_result_t result;
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT(result.output != NULL);
    HU_ASSERT(strstr(result.output, "pending") != NULL);

    hu_json_free(&test_alloc, args);

    if (result.output_owned && result.output) {
        test_alloc.free(test_alloc.ctx, (void *)result.output, result.output_len + 1);
    }

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

static void test_task_update_tool_execute(void) {
    setup_test_manager();

    uint32_t task_id = 0;
    hu_task_manager_add(test_mgr, &test_alloc, "Update me", 9, "Please update", 13, &task_id);

    hu_tool_t tool = hu_tool_task_update(&test_alloc, test_mgr);

    hu_json_value_t *args = hu_json_object_new(&test_alloc);
    hu_json_object_set(&test_alloc, args, "id", hu_json_number_new(&test_alloc, (double)task_id));
    hu_json_object_set(&test_alloc, args, "status", hu_json_string_new(&test_alloc, "in_progress", 11));

    hu_tool_result_t result;
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT(strstr(result.output, "in_progress") != NULL);

    const hu_task_t *task = NULL;
    hu_task_manager_get(test_mgr, task_id, &task);
    HU_ASSERT_EQ(task->status, HU_TASK_IN_PROGRESS);

    hu_json_free(&test_alloc, args);

    if (result.output_owned && result.output) {
        test_alloc.free(test_alloc.ctx, (void *)result.output, result.output_len + 1);
    }

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

static void test_task_list_tool_execute(void) {
    setup_test_manager();

    uint32_t id1, id2;
    hu_task_manager_add(test_mgr, &test_alloc, "Task 1", 6, "Description 1", 13, &id1);
    hu_task_manager_add(test_mgr, &test_alloc, "Task 2", 6, "Description 2", 13, &id2);

    hu_tool_t tool = hu_tool_task_list(&test_alloc, test_mgr);

    hu_json_value_t *args = hu_json_object_new(&test_alloc);
    hu_tool_result_t result;
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT(result.output != NULL);
    HU_ASSERT(strstr(result.output, "Task 1") != NULL);

    hu_json_free(&test_alloc, args);

    if (result.output_owned && result.output) {
        test_alloc.free(test_alloc.ctx, (void *)result.output, result.output_len + 1);
    }

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

static void test_task_get_tool_execute(void) {
    setup_test_manager();

    uint32_t task_id = 0;
    hu_task_manager_add(test_mgr, &test_alloc, "Get me", 6, "Please retrieve", 15, &task_id);

    hu_tool_t tool = hu_tool_task_get(&test_alloc, test_mgr);

    hu_json_value_t *args = hu_json_object_new(&test_alloc);
    hu_json_object_set(&test_alloc, args, "id", hu_json_number_new(&test_alloc, (double)task_id));

    hu_tool_result_t result;
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT(result.output != NULL);
    HU_ASSERT(strstr(result.output, "Get me") != NULL);

    hu_json_free(&test_alloc, args);

    if (result.output_owned && result.output) {
        test_alloc.free(test_alloc.ctx, (void *)result.output, result.output_len + 1);
    }

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

static void test_task_update_invalid_status(void) {
    setup_test_manager();

    uint32_t task_id = 0;
    hu_task_manager_add(test_mgr, &test_alloc, "Update me", 9, "Please update", 13, &task_id);

    hu_tool_t tool = hu_tool_task_update(&test_alloc, test_mgr);

    hu_json_value_t *args = hu_json_object_new(&test_alloc);
    hu_json_object_set(&test_alloc, args, "id", hu_json_number_new(&test_alloc, (double)task_id));
    hu_json_object_set(&test_alloc, args, "status", hu_json_string_new(&test_alloc, "invalid_status", 14));

    hu_tool_result_t result;
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, args, &result);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);

    hu_json_free(&test_alloc, args);

    if (tool.vtable->deinit) {
        tool.vtable->deinit(tool.ctx, &test_alloc);
    }

    teardown_test_manager();
}

void run_task_tools_tests(void) {
    test_alloc = hu_system_allocator();
    HU_TEST_SUITE("Task Tools");
    HU_RUN_TEST(test_task_create_tool_create);
    HU_RUN_TEST(test_task_create_tool_name);
    HU_RUN_TEST(test_task_create_execute);
    HU_RUN_TEST(test_task_update_tool_execute);
    HU_RUN_TEST(test_task_list_tool_execute);
    HU_RUN_TEST(test_task_get_tool_execute);
    HU_RUN_TEST(test_task_update_invalid_status);
}
