#include "human/core/allocator.h"
#include "human/task_manager.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static hu_allocator_t test_alloc;

static void test_task_manager_create(void) {
    hu_task_manager_t *mgr = NULL;
    hu_error_t err = hu_task_manager_create(&test_alloc, 100, &mgr);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(mgr != NULL);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_add(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    uint32_t task_id = 0;
    hu_error_t err = hu_task_manager_add(mgr, &test_alloc, "Buy groceries", 13,
                                         "Buy milk, eggs, and bread", 26, &task_id);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_GT(task_id, 0);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_get(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    uint32_t task_id = 0;
    hu_task_manager_add(mgr, &test_alloc, "Write code", 10, "Fix the bug in module X", 23, &task_id);

    const hu_task_t *task = NULL;
    hu_error_t err = hu_task_manager_get(mgr, task_id, &task);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(task != NULL);
    HU_ASSERT_EQ(task->id, task_id);
    HU_ASSERT_EQ(strcmp(task->subject, "Write code"), 0);
    HU_ASSERT_EQ(task->status, HU_TASK_PENDING);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_update_status(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    uint32_t task_id = 0;
    hu_task_manager_add(mgr, &test_alloc, "Review PR", 9, "Review PR #123", 14, &task_id);

    hu_error_t err = hu_task_manager_update_status(mgr, task_id, HU_TASK_IN_PROGRESS);
    HU_ASSERT_EQ(err, HU_OK);

    const hu_task_t *task = NULL;
    hu_task_manager_get(mgr, task_id, &task);
    HU_ASSERT_EQ(task->status, HU_TASK_IN_PROGRESS);

    err = hu_task_manager_update_status(mgr, task_id, HU_TASK_COMPLETED);
    HU_ASSERT_EQ(err, HU_OK);

    hu_task_manager_get(mgr, task_id, &task);
    HU_ASSERT_EQ(task->status, HU_TASK_COMPLETED);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_list(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    uint32_t id1, id2, id3;
    hu_task_manager_add(mgr, &test_alloc, "Task 1", 6, "First task", 10, &id1);
    hu_task_manager_add(mgr, &test_alloc, "Task 2", 6, "Second task", 11, &id2);
    hu_task_manager_add(mgr, &test_alloc, "Task 3", 6, "Third task", 10, &id3);

    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_task_manager_list(mgr, &test_alloc, &json, &json_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(json != NULL);
    HU_ASSERT_GT(json_len, 0);
    HU_ASSERT(strstr(json, "Task 1") != NULL);
    HU_ASSERT(strstr(json, "Task 2") != NULL);

    test_alloc.free(test_alloc.ctx, json, json_len + 1);
    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_multiple_tasks(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    uint32_t id1, id2, id3;
    hu_task_manager_add(mgr, &test_alloc, "Task 1", 6, "Description 1", 13, &id1);
    hu_task_manager_add(mgr, &test_alloc, "Task 2", 6, "Description 2", 13, &id2);
    hu_task_manager_add(mgr, &test_alloc, "Task 3", 6, "Description 3", 13, &id3);

    const hu_task_t *t1, *t2, *t3;
    hu_task_manager_get(mgr, id1, &t1);
    hu_task_manager_get(mgr, id2, &t2);
    hu_task_manager_get(mgr, id3, &t3);

    HU_ASSERT_EQ(t1->id, id1);
    HU_ASSERT_EQ(t2->id, id2);
    HU_ASSERT_EQ(t3->id, id3);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_get_nonexistent(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    uint32_t id1;
    hu_task_manager_add(mgr, &test_alloc, "Task 1", 6, "Description 1", 13, &id1);

    const hu_task_t *task = NULL;
    hu_error_t err = hu_task_manager_get(mgr, 9999, &task);

    HU_ASSERT_NEQ(err, HU_OK);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_update_nonexistent(void) {
    hu_task_manager_t *mgr = NULL;
    hu_task_manager_create(&test_alloc, 100, &mgr);

    hu_error_t err = hu_task_manager_update_status(mgr, 9999, HU_TASK_COMPLETED);

    HU_ASSERT_NEQ(err, HU_OK);

    hu_task_manager_destroy(mgr, &test_alloc);
}

static void test_task_manager_null_args(void) {
    hu_task_manager_t **out = NULL;
    hu_error_t err = hu_task_manager_create(NULL, 100, out);
    HU_ASSERT_NEQ(err, HU_OK);
}

void run_task_manager_tests(void) {
    test_alloc = hu_system_allocator();
    HU_TEST_SUITE("Task Manager");
    HU_RUN_TEST(test_task_manager_create);
    HU_RUN_TEST(test_task_manager_add);
    HU_RUN_TEST(test_task_manager_get);
    HU_RUN_TEST(test_task_manager_update_status);
    HU_RUN_TEST(test_task_manager_list);
    HU_RUN_TEST(test_task_manager_multiple_tasks);
    HU_RUN_TEST(test_task_manager_get_nonexistent);
    HU_RUN_TEST(test_task_manager_update_nonexistent);
    HU_RUN_TEST(test_task_manager_null_args);
}
