#include "test_framework.h"
#include "human/experience.h"
#ifdef HU_ENABLE_SQLITE
#include "human/memory.h"
#endif
#include <string.h>

static void test_experience_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_experience_store_t store;
    HU_ASSERT_EQ(hu_experience_store_init(&alloc, NULL, &store), HU_OK);
    HU_ASSERT(store.alloc != NULL);
    HU_ASSERT_EQ(store.stored_count, (size_t)0);
    hu_experience_store_deinit(&store);
    HU_ASSERT(store.alloc == NULL);
}

static void test_experience_record(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_experience_store_t store;
    HU_ASSERT_EQ(hu_experience_store_init(&alloc, NULL, &store), HU_OK);
    HU_ASSERT_EQ(hu_experience_record(&store, "send email", 10, "used gmail", 10, "delivered", 9, 0.95), HU_OK);
    HU_ASSERT_EQ(store.stored_count, (size_t)1);
    HU_ASSERT_EQ(hu_experience_record(&store, "schedule", 8, "opened cal", 10, "created", 7, 0.88), HU_OK);
    HU_ASSERT_EQ(store.stored_count, (size_t)2);
    hu_experience_store_deinit(&store);
}

static void test_experience_recall(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_experience_store_t store;
    hu_experience_store_init(&alloc, NULL, &store);
    char *ctx = NULL; size_t ctx_len = 0;
    HU_ASSERT_EQ(hu_experience_recall_similar(&store, "task", 4, &ctx, &ctx_len), HU_OK);
    HU_ASSERT(ctx == NULL);
    hu_experience_record(&store, "task", 4, "action", 6, "result", 6, 0.9);
    HU_ASSERT_EQ(hu_experience_recall_similar(&store, "task", 4, &ctx, &ctx_len), HU_OK);
    HU_ASSERT(ctx != NULL);
    HU_ASSERT(ctx_len > 0);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    hu_experience_store_deinit(&store);
}

static void test_experience_build_prompt(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_experience_store_t store;
    hu_experience_store_init(&alloc, NULL, &store);
    char *prompt = NULL;
    size_t plen = 0;
    HU_ASSERT_EQ(hu_experience_build_prompt(&store, "current task", 12, &prompt, &plen), HU_OK);
    HU_ASSERT(prompt != NULL);
    HU_ASSERT(plen == 0);
    alloc.free(alloc.ctx, prompt, 1);
    hu_experience_record(&store, "current task", 12, "action", 6, "result", 6, 0.9);
    HU_ASSERT_EQ(hu_experience_build_prompt(&store, "current task", 12, &prompt, &plen), HU_OK);
    HU_ASSERT(prompt != NULL);
    HU_ASSERT(plen > 0);
    alloc.free(alloc.ctx, prompt, plen + 1);
    hu_experience_store_deinit(&store);
}

static void test_experience_null_args(void) {
    hu_experience_store_t store;
    HU_ASSERT_EQ(hu_experience_store_init(NULL, NULL, &store), HU_ERR_INVALID_ARGUMENT);
}

#ifdef HU_ENABLE_SQLITE
static void test_experience_memory_store_recall(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);
    hu_experience_store_t store;
    HU_ASSERT_EQ(hu_experience_store_init(&alloc, &mem, &store), HU_OK);
    HU_ASSERT_EQ(hu_experience_record(&store, "send email", 10, "used gmail", 10, "delivered", 9,
                                      0.95),
                 HU_OK);
    HU_ASSERT_EQ(store.stored_count, (size_t)1);
    char *ctx = NULL;
    size_t ctx_len = 0;
    HU_ASSERT_EQ(hu_experience_recall_similar(&store, "send email", 10, &ctx, &ctx_len), HU_OK);
    HU_ASSERT(ctx != NULL);
    HU_ASSERT(ctx_len > 0);
    HU_ASSERT_TRUE(ctx_len >= 40);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    hu_experience_store_deinit(&store);
    mem.vtable->deinit(mem.ctx);
}
#endif

static void test_experience_record_recall_verifies_content(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_experience_store_t store;
    hu_experience_store_init(&alloc, NULL, &store);

    hu_experience_record(&store, "debug segfault", 14,
                         "used gdb backtrace", 18,
                         "found null pointer", 18, 0.95);
    hu_experience_record(&store, "optimize query", 14,
                         "added index", 11,
                         "50x speedup", 11, 0.99);

    char *ctx = NULL;
    size_t ctx_len = 0;
    HU_ASSERT_EQ(hu_experience_recall_similar(&store, "debug crash", 11, &ctx, &ctx_len), HU_OK);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(ctx_len > 0);
    HU_ASSERT_TRUE(strstr(ctx, "debug") != NULL || strstr(ctx, "segfault") != NULL ||
                   strstr(ctx, "gdb") != NULL || strstr(ctx, "null pointer") != NULL);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);

    ctx = NULL;
    ctx_len = 0;
    HU_ASSERT_EQ(hu_experience_recall_similar(&store, "optimize database", 17, &ctx, &ctx_len), HU_OK);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(ctx_len > 0);
    HU_ASSERT_TRUE(strstr(ctx, "optimize") != NULL || strstr(ctx, "index") != NULL ||
                   strstr(ctx, "speedup") != NULL);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);

    hu_experience_store_deinit(&store);
}

void run_experience_tests(void) {
    HU_TEST_SUITE("Experience Store");
    HU_RUN_TEST(test_experience_init_deinit);
    HU_RUN_TEST(test_experience_record);
    HU_RUN_TEST(test_experience_recall);
    HU_RUN_TEST(test_experience_build_prompt);
    HU_RUN_TEST(test_experience_null_args);
    HU_RUN_TEST(test_experience_record_recall_verifies_content);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(test_experience_memory_store_recall);
#endif
}
