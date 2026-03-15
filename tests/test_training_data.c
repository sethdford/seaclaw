/* Agent training data collection tests (AGI-V3). */

typedef int hu_test_training_data_unused_;

#ifdef HU_ENABLE_SQLITE

#include "human/core/allocator.h"
#include <sqlite3.h>
#include "human/ml/training_data.h"
#include "human/memory.h"
#include "test_framework.h"
#include <string.h>

static void training_data_init_tables(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_error_t err = hu_training_data_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

static void training_data_start_and_end_trajectory(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_error_t err = hu_training_data_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    int64_t tid = 0;
    err = hu_training_data_start_trajectory(&alloc, db, &tid);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(tid > 0);

    err = hu_training_data_record_step(&alloc, db, tid, "state1", 6, "action1", 7, 0.5, HU_REWARD_TASK_SUCCESS);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_training_data_record_step(&alloc, db, tid, "state2", 6, "action2", 7, 0.3, HU_REWARD_USER_FEEDBACK);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_training_data_end_trajectory(db, tid, 0.8);
    HU_ASSERT_EQ(err, HU_OK);

    size_t count = 0;
    err = hu_training_data_count(db, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1u);

    mem.vtable->deinit(mem.ctx);
}

static void training_data_record_step(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_error_t err = hu_training_data_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    int64_t tid = 0;
    err = hu_training_data_start_trajectory(&alloc, db, &tid);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_training_data_record_step(&alloc, db, tid, "observed_state", 14, "tool_call", 9, 1.0, HU_REWARD_TOOL_SUCCESS);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_training_data_end_trajectory(db, tid, 1.0);

    char *json = NULL;
    size_t json_len = 0;
    err = hu_training_data_export_json(&alloc, db, &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    HU_ASSERT_TRUE(json_len > 0);
    HU_ASSERT_TRUE(strstr(json, "observed_state") != NULL);
    HU_ASSERT_TRUE(strstr(json, "tool_call") != NULL);

    alloc.free(alloc.ctx, json, json_len + 1);
    mem.vtable->deinit(mem.ctx);
}

static void training_data_export_json(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_error_t err = hu_training_data_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    int64_t tid = 0;
    err = hu_training_data_start_trajectory(&alloc, db, &tid);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_training_data_record_step(&alloc, db, tid, "s", 1, "a", 1, 0.0, HU_REWARD_SELF_EVAL);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_training_data_end_trajectory(db, tid, 0.0);
    HU_ASSERT_EQ(err, HU_OK);

    char *json = NULL;
    size_t json_len = 0;
    err = hu_training_data_export_json(&alloc, db, &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    HU_ASSERT_TRUE(json_len > 0);
    HU_ASSERT_TRUE(json[0] == '[');

    alloc.free(alloc.ctx, json, json_len + 1);
    mem.vtable->deinit(mem.ctx);
}

static void training_data_strip_pii_email(void)
{
    char buf[256];
    strncpy(buf, "Contact user@example.com for details", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    size_t out_len = 0;

    hu_error_t err = hu_training_data_strip_pii(buf, len, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "[EMAIL]") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "user@example.com") == NULL);
}

static void training_data_strip_pii_phone(void)
{
    char buf[256];
    strncpy(buf, "Call 555-123-4567 for support", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    size_t out_len = 0;

    hu_error_t err = hu_training_data_strip_pii(buf, len, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "[PHONE]") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "555-123-4567") == NULL);
}

static void training_data_count_only_complete(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_error_t err = hu_training_data_init_tables(db);
    HU_ASSERT_EQ(err, HU_OK);

    int64_t tid1 = 0, tid2 = 0;
    err = hu_training_data_start_trajectory(&alloc, db, &tid1);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_training_data_start_trajectory(&alloc, db, &tid2);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_training_data_end_trajectory(db, tid1, 0.5);
    HU_ASSERT_EQ(err, HU_OK);
    /* tid2 not ended — incomplete */

    size_t count = 0;
    err = hu_training_data_count(db, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 1u);

    mem.vtable->deinit(mem.ctx);
}

static void training_data_null_args_returns_error(void)
{
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);

    hu_error_t err = hu_training_data_init_tables(NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    int64_t tid = 0;
    err = hu_training_data_start_trajectory(NULL, db, &tid);
    HU_ASSERT_NEQ(err, HU_OK);
    err = hu_training_data_start_trajectory(&alloc, NULL, &tid);
    HU_ASSERT_NEQ(err, HU_OK);
    err = hu_training_data_start_trajectory(&alloc, db, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    err = hu_training_data_end_trajectory(NULL, 1, 0.0);
    HU_ASSERT_NEQ(err, HU_OK);

    char *json = NULL;
    size_t json_len = 0;
    err = hu_training_data_export_json(NULL, db, &json, &json_len);
    HU_ASSERT_NEQ(err, HU_OK);
    err = hu_training_data_export_json(&alloc, db, NULL, &json_len);
    HU_ASSERT_NEQ(err, HU_OK);

    size_t count = 0;
    err = hu_training_data_count(NULL, &count);
    HU_ASSERT_NEQ(err, HU_OK);
    err = hu_training_data_count(db, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    char buf[64];
    size_t out_len = 0;
    err = hu_training_data_strip_pii(NULL, 5, &out_len);
    HU_ASSERT_NEQ(err, HU_OK);
    err = hu_training_data_strip_pii(buf, 5, NULL);
    HU_ASSERT_NEQ(err, HU_OK);

    mem.vtable->deinit(mem.ctx);
}

void run_training_data_tests(void)
{
    HU_TEST_SUITE("training_data");
    HU_RUN_TEST(training_data_init_tables);
    HU_RUN_TEST(training_data_start_and_end_trajectory);
    HU_RUN_TEST(training_data_record_step);
    HU_RUN_TEST(training_data_export_json);
    HU_RUN_TEST(training_data_strip_pii_email);
    HU_RUN_TEST(training_data_strip_pii_phone);
    HU_RUN_TEST(training_data_count_only_complete);
    HU_RUN_TEST(training_data_null_args_returns_error);
}

#else

void run_training_data_tests(void)
{
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
