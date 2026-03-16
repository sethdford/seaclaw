/*
 * Tests for src/channels/dispatch.c — message routing between channels.
 * Requires HU_HAS_DISPATCH.
 */

#include "human/channels/dispatch.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

#if HU_HAS_DISPATCH

static void test_dispatch_create_null_allocator_returns_error(void) {
    hu_channel_t ch = {0};
    hu_error_t err = hu_dispatch_create(NULL, &ch);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_dispatch_create_null_out_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_dispatch_create(&alloc, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_dispatch_create_valid_allocator_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_error_t err = hu_dispatch_create(&alloc, &ch);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(ch.ctx);
    HU_ASSERT_NOT_NULL(ch.vtable);
    hu_dispatch_destroy(&ch);
}

static void test_dispatch_name_returns_dispatch(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_dispatch_create(&alloc, &ch);
    HU_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "dispatch");
    hu_dispatch_destroy(&ch);
}

static void test_dispatch_destroy_null_does_not_crash(void) {
    hu_dispatch_destroy(NULL);
}

static void test_dispatch_send_zero_subchannels(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_dispatch_create(&alloc, &ch);
    /* In HU_IS_TEST mode, send always returns HU_OK (no real network).
       The NOT_SUPPORTED path for 0 sub-channels is only in non-test builds. */
    hu_error_t err = ch.vtable->send(ch.ctx, "target", 6, "msg", 3, NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
    hu_dispatch_destroy(&ch);
}

static void test_dispatch_add_channel_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t dispatch_ch = {0};
    hu_channel_t sub = {0};
    sub.ctx = (void *)0x1;
    sub.vtable = NULL;
    hu_dispatch_create(&alloc, &dispatch_ch);
    hu_error_t err = hu_dispatch_add_channel(&dispatch_ch, &sub);
    HU_ASSERT_EQ(err, HU_OK);
    hu_dispatch_destroy(&dispatch_ch);
}

static void test_dispatch_add_channel_null_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t dispatch_ch = {0};
    hu_dispatch_create(&alloc, &dispatch_ch);
    hu_error_t err = hu_dispatch_add_channel(&dispatch_ch, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_dispatch_destroy(&dispatch_ch);
}

static void test_dispatch_start_stop_health(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_dispatch_create(&alloc, &ch);
    HU_ASSERT_FALSE(ch.vtable->health_check(ch.ctx));
    HU_ASSERT_EQ(ch.vtable->start(ch.ctx), HU_OK);
    HU_ASSERT_TRUE(ch.vtable->health_check(ch.ctx));
    ch.vtable->stop(ch.ctx);
    HU_ASSERT_FALSE(ch.vtable->health_check(ch.ctx));
    hu_dispatch_destroy(&ch);
}

void run_dispatch_tests(void) {
    HU_TEST_SUITE("dispatch");
    HU_RUN_TEST(test_dispatch_create_null_allocator_returns_error);
    HU_RUN_TEST(test_dispatch_create_null_out_returns_error);
    HU_RUN_TEST(test_dispatch_create_valid_allocator_succeeds);
    HU_RUN_TEST(test_dispatch_name_returns_dispatch);
    HU_RUN_TEST(test_dispatch_destroy_null_does_not_crash);
    HU_RUN_TEST(test_dispatch_send_zero_subchannels);
    HU_RUN_TEST(test_dispatch_add_channel_succeeds);
    HU_RUN_TEST(test_dispatch_add_channel_null_fails);
    HU_RUN_TEST(test_dispatch_start_stop_health);
}

#else

void run_dispatch_tests(void) {
    HU_TEST_SUITE("dispatch");
    /* No tests when HU_HAS_DISPATCH is off */
}

#endif
