#include "human/gateway/thread_pool.h"
#include "test_framework.h"
#include <string.h>
#include <unistd.h>

static volatile int counter;

static void increment_counter(void *arg) {
    (void)arg;
    __atomic_fetch_add(&counter, 1, __ATOMIC_SEQ_CST);
}

static void thread_pool_create_null_returns_null(void) {
    HU_ASSERT_NULL(hu_thread_pool_create(0));
}

static void thread_pool_create_and_destroy(void) {
    hu_thread_pool_t *pool = hu_thread_pool_create(2);
    HU_ASSERT_NOT_NULL(pool);
    hu_thread_pool_destroy(pool);
}

static void thread_pool_destroy_null_is_safe(void) {
    hu_thread_pool_destroy(NULL);
}

static void thread_pool_submit_null_pool_returns_false(void) {
    HU_ASSERT_FALSE(hu_thread_pool_submit(NULL, increment_counter, NULL));
}

static void thread_pool_submit_null_fn_returns_false(void) {
    hu_thread_pool_t *pool = hu_thread_pool_create(1);
    HU_ASSERT_NOT_NULL(pool);
    HU_ASSERT_FALSE(hu_thread_pool_submit(pool, NULL, NULL));
    hu_thread_pool_destroy(pool);
}

static void thread_pool_submit_and_execute(void) {
    __atomic_store_n(&counter, 0, __ATOMIC_SEQ_CST);
    hu_thread_pool_t *pool = hu_thread_pool_create(2);
    HU_ASSERT_NOT_NULL(pool);
    for (int i = 0; i < 10; i++)
        HU_ASSERT_TRUE(hu_thread_pool_submit(pool, increment_counter, NULL));
    hu_thread_pool_destroy(pool);
    HU_ASSERT_EQ(__atomic_load_n(&counter, __ATOMIC_SEQ_CST), 10);
}

static void thread_pool_active_null_returns_zero(void) {
    HU_ASSERT_EQ((int)hu_thread_pool_active(NULL), 0);
}

void run_thread_pool_tests(void) {
    HU_TEST_SUITE("ThreadPool");

    HU_RUN_TEST(thread_pool_create_null_returns_null);
    HU_RUN_TEST(thread_pool_create_and_destroy);
    HU_RUN_TEST(thread_pool_destroy_null_is_safe);
    HU_RUN_TEST(thread_pool_submit_null_pool_returns_false);
    HU_RUN_TEST(thread_pool_submit_null_fn_returns_false);
    HU_RUN_TEST(thread_pool_submit_and_execute);
    HU_RUN_TEST(thread_pool_active_null_returns_zero);
}
