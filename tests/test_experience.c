#include "test_framework.h"
#include "human/experience.h"

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
    char *prompt = NULL; size_t plen = 0;
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

void run_experience_tests(void) {
    HU_TEST_SUITE("Experience Store");
    HU_RUN_TEST(test_experience_init_deinit);
    HU_RUN_TEST(test_experience_record);
    HU_RUN_TEST(test_experience_recall);
    HU_RUN_TEST(test_experience_build_prompt);
    HU_RUN_TEST(test_experience_null_args);
}
