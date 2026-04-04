#include "human/agent/task_store.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void cp_tasks_task_status_strings(void) {
    HU_ASSERT_STR_EQ(hu_task_status_string(HU_TASK_STATUS_PENDING), "pending");
    HU_ASSERT_STR_EQ(hu_task_status_string(HU_TASK_STATUS_RUNNING), "running");
    HU_ASSERT_STR_EQ(hu_task_status_string(HU_TASK_STATUS_COMPLETED), "completed");
    HU_ASSERT_STR_EQ(hu_task_status_string(HU_TASK_STATUS_FAILED), "failed");
    HU_ASSERT_STR_EQ(hu_task_status_string(HU_TASK_STATUS_CANCELLED), "cancelled");
}

static void cp_tasks_store_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_task_store_t *st = NULL;
    HU_ASSERT_EQ(hu_task_store_create(&alloc, NULL, &st), HU_OK);
    hu_task_record_t rec = {.id = 0, .name = (char *)"test-task",
                            .status = HU_TASK_STATUS_PENDING,
                            .program_json = (char *)"{\"op\":\"noop\"}",
                            .trace_json = (char *)"[]", .parent_task_id = 0};
    uint64_t nid = 0;
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &rec, &nid), HU_OK);
    HU_ASSERT_TRUE(nid > 0);
    hu_task_record_t out = {0};
    HU_ASSERT_EQ(hu_task_store_load(st, &alloc, nid, &out), HU_OK);
    HU_ASSERT_STR_EQ(out.name, "test-task");
    HU_ASSERT_EQ(out.status, HU_TASK_STATUS_PENDING);
    HU_ASSERT_EQ(hu_task_store_update_status(st, nid, HU_TASK_STATUS_CANCELLED), HU_OK);
    hu_task_record_t out2 = {0};
    HU_ASSERT_EQ(hu_task_store_load(st, &alloc, nid, &out2), HU_OK);
    HU_ASSERT_EQ(out2.status, HU_TASK_STATUS_CANCELLED);
    hu_task_record_free(&alloc, &out);
    hu_task_record_free(&alloc, &out2);
    hu_task_store_destroy(st, &alloc);
}

static void cp_tasks_store_list_filter(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_task_store_t *st = NULL;
    HU_ASSERT_EQ(hu_task_store_create(&alloc, NULL, &st), HU_OK);
    hu_task_record_t r1 = {.name = (char *)"a", .status = HU_TASK_STATUS_PENDING};
    hu_task_record_t r2 = {.name = (char *)"b", .status = HU_TASK_STATUS_RUNNING};
    uint64_t id1 = 0, id2 = 0;
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &r1, &id1), HU_OK);
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &r2, &id2), HU_OK);
    hu_task_record_t *out = NULL;
    size_t count = 0;
    hu_task_status_t filter = HU_TASK_STATUS_RUNNING;
    HU_ASSERT_EQ(hu_task_store_list(st, &alloc, &filter, &out, &count), HU_OK);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_STR_EQ(out[0].name, "b");
    hu_task_records_free(&alloc, out, count);
    hu_task_store_destroy(st, &alloc);
}

void run_cp_tasks_tests(void) {
    HU_TEST_SUITE("CpTasks");
    HU_RUN_TEST(cp_tasks_task_status_strings);
    HU_RUN_TEST(cp_tasks_store_roundtrip);
    HU_RUN_TEST(cp_tasks_store_list_filter);
}
