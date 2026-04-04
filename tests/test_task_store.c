#include "human/agent/task_store.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void task_store_create_destroy_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_task_store_t *st = NULL;
    HU_ASSERT_EQ(hu_task_store_create(&alloc, NULL, &st), HU_OK);
    HU_ASSERT_NOT_NULL(st);
    hu_task_store_destroy(st, &alloc);
}

static void task_store_save_load_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_task_store_t *st = NULL;
    HU_ASSERT_EQ(hu_task_store_create(&alloc, NULL, &st), HU_OK);

    hu_task_record_t in = {.id = 0,
                            .name = (char *)"alpha",
                            .status = HU_TASK_STATUS_PENDING,
                            .program_json = (char *)"{\"op\":\"noop\"}",
                            .trace_json = (char *)"[]",
                            .parent_task_id = 0};
    uint64_t nid = 0;
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &in, &nid), HU_OK);
    HU_ASSERT_TRUE(nid > 0);

    hu_task_record_t out = {0};
    HU_ASSERT_EQ(hu_task_store_load(st, &alloc, nid, &out), HU_OK);
    HU_ASSERT_EQ(out.id, nid);
    HU_ASSERT_STR_EQ(out.name, "alpha");
    HU_ASSERT_EQ(out.status, HU_TASK_STATUS_PENDING);
    HU_ASSERT_NOT_NULL(out.program_json);
    HU_ASSERT_STR_EQ(out.program_json, "{\"op\":\"noop\"}");
    HU_ASSERT_NOT_NULL(out.trace_json);
    HU_ASSERT_STR_EQ(out.trace_json, "[]");

    hu_task_record_free(&alloc, &out);
    hu_task_store_destroy(st, &alloc);
}

static void task_store_update_status_changes_row(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_task_store_t *st = NULL;
    HU_ASSERT_EQ(hu_task_store_create(&alloc, NULL, &st), HU_OK);

    hu_task_record_t in = {.id = 0,
                            .name = (char *)"job",
                            .status = HU_TASK_STATUS_PENDING,
                            .program_json = NULL,
                            .trace_json = NULL,
                            .parent_task_id = 0};
    uint64_t nid = 0;
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &in, &nid), HU_OK);

    HU_ASSERT_EQ(hu_task_store_update_status(st, nid, HU_TASK_STATUS_RUNNING), HU_OK);

    hu_task_record_t out = {0};
    HU_ASSERT_EQ(hu_task_store_load(st, &alloc, nid, &out), HU_OK);
    HU_ASSERT_EQ(out.status, HU_TASK_STATUS_RUNNING);

    hu_task_record_free(&alloc, &out);
    hu_task_store_destroy(st, &alloc);
}

static void task_store_list_respects_status_filter(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_task_store_t *st = NULL;
    HU_ASSERT_EQ(hu_task_store_create(&alloc, NULL, &st), HU_OK);

    hu_task_record_t a = {.id = 0,
                          .name = (char *)"p1",
                          .status = HU_TASK_STATUS_PENDING,
                          .program_json = NULL,
                          .trace_json = NULL,
                          .parent_task_id = 0};
    hu_task_record_t b = {.id = 0,
                          .name = (char *)"r1",
                          .status = HU_TASK_STATUS_RUNNING,
                          .program_json = NULL,
                          .trace_json = NULL,
                          .parent_task_id = 0};
    uint64_t ida = 0, idb = 0;
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &a, &ida), HU_OK);
    HU_ASSERT_EQ(hu_task_store_save(st, &alloc, &b, &idb), HU_OK);

    hu_task_status_t flt = HU_TASK_STATUS_PENDING;
    hu_task_record_t *rows = NULL;
    size_t n = 0;
    HU_ASSERT_EQ(hu_task_store_list(st, &alloc, &flt, &rows, &n), HU_OK);
    HU_ASSERT_EQ(n, 1u);
    HU_ASSERT_EQ(rows[0].status, HU_TASK_STATUS_PENDING);
    HU_ASSERT_STR_EQ(rows[0].name, "p1");

    hu_task_records_free(&alloc, rows, n);

    HU_ASSERT_EQ(hu_task_store_list(st, &alloc, NULL, &rows, &n), HU_OK);
    HU_ASSERT_EQ(n, 2u);
    hu_task_records_free(&alloc, rows, n);

    hu_task_store_destroy(st, &alloc);
}

void run_task_store_tests(void) {
    HU_TEST_SUITE("task_store");
    HU_RUN_TEST(task_store_create_destroy_succeeds);
    HU_RUN_TEST(task_store_save_load_roundtrip);
    HU_RUN_TEST(task_store_update_status_changes_row);
    HU_RUN_TEST(task_store_list_respects_status_filter);
}
