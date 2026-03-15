#ifdef HU_ENABLE_SQLITE

#include "human/core/allocator.h"
#include "human/memory.h"
#include "human/memory/prospective.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static void prospective_schedule_time_trigger(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t id = 0;
    int64_t future_ts = (int64_t)time(NULL) + 86400;
    char tv[32];
    int len = (int)snprintf(tv, sizeof(tv), "%lld", (long long)future_ts);
    HU_ASSERT_TRUE(len > 0 && (size_t)len < sizeof(tv));

    hu_error_t err = hu_prospective_schedule(db, "Remind about meeting", 20, "time", 4, tv,
                                              (size_t)len, 0.5, &id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(id > 0);

    mem.vtable->deinit(mem.ctx);
}

static void prospective_check_time_trigger_fires(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t past_ts = (int64_t)time(NULL) - 3600;
    char tv[32];
    int len = (int)snprintf(tv, sizeof(tv), "%lld", (long long)past_ts);
    HU_ASSERT_TRUE(len > 0);

    int64_t id = 0;
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Past task", 9, "time", 4, tv, (size_t)len, 0.5, &id),
                 HU_OK);

    hu_prospective_task_t *out = NULL;
    size_t count = 0;
    int64_t now_ts = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_prospective_task_check_triggers(&alloc, db, "time", tv, (size_t)len, now_ts,
                                                   &out, &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_EQ(out[0].description, "Past task");
    HU_ASSERT_EQ(out[0].id, id);

    alloc.free(alloc.ctx, out, count * sizeof(hu_prospective_task_t));
    mem.vtable->deinit(mem.ctx);
}

static void prospective_check_time_trigger_not_yet(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t future_ts = (int64_t)time(NULL) + 86400;
    char tv[32];
    int len = (int)snprintf(tv, sizeof(tv), "%lld", (long long)future_ts);
    HU_ASSERT_TRUE(len > 0);

    int64_t id = 0;
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Future task", 11, "time", 4, tv, (size_t)len, 0.5,
                                          &id),
                 HU_OK);

    hu_prospective_task_t *out = NULL;
    size_t count = 0;
    int64_t now_ts = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_prospective_task_check_triggers(&alloc, db, "time", tv, (size_t)len, now_ts,
                                                   &out, &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 0u);
    HU_ASSERT_NULL(out);

    mem.vtable->deinit(mem.ctx);
}

static void prospective_mark_fired_updates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t past_ts = (int64_t)time(NULL) - 3600;
    char tv[32];
    int len = (int)snprintf(tv, sizeof(tv), "%lld", (long long)past_ts);
    HU_ASSERT_TRUE(len > 0);

    int64_t id = 0;
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Task to fire", 12, "time", 4, tv, (size_t)len, 0.5,
                                          &id),
                 HU_OK);

    hu_prospective_task_t *out = NULL;
    size_t count = 0;
    int64_t now_ts = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_prospective_task_check_triggers(&alloc, db, "time", tv, (size_t)len, now_ts,
                                                   &out, &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 1u);
    int64_t found_id = out[0].id;
    alloc.free(alloc.ctx, out, count * sizeof(hu_prospective_task_t));

    HU_ASSERT_EQ(hu_prospective_task_mark_fired(db, found_id), HU_OK);

    out = NULL;
    count = 0;
    HU_ASSERT_EQ(hu_prospective_task_check_triggers(&alloc, db, "time", tv, (size_t)len, now_ts,
                                                   &out, &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 0u);
    HU_ASSERT_NULL(out);

    mem.vtable->deinit(mem.ctx);
}

static void prospective_check_event_trigger(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t id = 0;
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Send birthday card", 18, "event", 5, "birthday", 8,
                                         0.8, &id),
                 HU_OK);

    hu_prospective_task_t *out = NULL;
    size_t count = 0;
    int64_t now_ts = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_prospective_task_check_triggers(&alloc, db, "event", "birthday", 8, now_ts,
                                                   &out, &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_EQ(out[0].description, "Send birthday card");
    HU_ASSERT_STR_EQ(out[0].trigger_value, "birthday");

    alloc.free(alloc.ctx, out, count * sizeof(hu_prospective_task_t));
    mem.vtable->deinit(mem.ctx);
}

static void prospective_schedule_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t id = 0;
    HU_ASSERT_EQ(hu_prospective_schedule(NULL, "desc", 4, "time", 4, "0", 1, 0.5, &id),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prospective_schedule(db, NULL, 4, "time", 4, "0", 1, 0.5, &id),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prospective_schedule(db, "desc", 4, NULL, 4, "0", 1, 0.5, &id),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prospective_schedule(db, "desc", 4, "time", 4, NULL, 1, 0.5, &id),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prospective_schedule(db, "desc", 4, "time", 4, "0", 1, 0.5, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_prospective_schedule(db, "desc", 4, "invalid", 7, "0", 1, 0.5, &id),
                 HU_ERR_INVALID_ARGUMENT);

    mem.vtable->deinit(mem.ctx);
}

static void prospective_multiple_triggers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    int64_t past_ts = (int64_t)time(NULL) - 3600;
    char past_tv[32];
    int past_len = (int)snprintf(past_tv, sizeof(past_tv), "%lld", (long long)past_ts);
    HU_ASSERT_TRUE(past_len > 0);

    int64_t future_ts = (int64_t)time(NULL) + 86400;
    char future_tv[32];
    int future_len = (int)snprintf(future_tv, sizeof(future_tv), "%lld", (long long)future_ts);
    HU_ASSERT_TRUE(future_len > 0);

    int64_t id1 = 0, id2 = 0, id3 = 0;
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Past task 1", 11, "time", 4, past_tv,
                                         (size_t)past_len, 0.5, &id1),
                 HU_OK);
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Past task 2", 11, "time", 4, past_tv,
                                         (size_t)past_len, 0.7, &id2),
                 HU_OK);
    HU_ASSERT_EQ(hu_prospective_schedule(db, "Future task", 11, "time", 4, future_tv,
                                         (size_t)future_len, 0.5, &id3),
                 HU_OK);

    hu_prospective_task_t *out = NULL;
    size_t count = 0;
    int64_t now_ts = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_prospective_task_check_triggers(&alloc, db, "time", past_tv,
                                                    (size_t)past_len, now_ts, &out, &count),
                 HU_OK);
    HU_ASSERT_EQ(count, 2u);
    HU_ASSERT_NOT_NULL(out);

    alloc.free(alloc.ctx, out, count * sizeof(hu_prospective_task_t));
    mem.vtable->deinit(mem.ctx);
}

void run_prospective_memory_tests(void) {
    HU_TEST_SUITE("prospective_memory");
    HU_RUN_TEST(prospective_schedule_time_trigger);
    HU_RUN_TEST(prospective_check_time_trigger_fires);
    HU_RUN_TEST(prospective_check_time_trigger_not_yet);
    HU_RUN_TEST(prospective_mark_fired_updates);
    HU_RUN_TEST(prospective_check_event_trigger);
    HU_RUN_TEST(prospective_schedule_null_args);
    HU_RUN_TEST(prospective_multiple_triggers);
}

#else

void run_prospective_memory_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
