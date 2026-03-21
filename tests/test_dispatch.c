/*
 * Tests for src/channels/dispatch.c — message routing between channels.
 * Requires HU_HAS_DISPATCH.
 */

#include "human/channel.h"
#include "human/channels/dispatch.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

typedef struct mock_route_channel_ctx {
    const char *channel_name;
    unsigned send_count;
    char last_target[128];
    size_t last_target_len;
    char last_message[128];
    size_t last_message_len;
} mock_route_channel_ctx_t;

static hu_error_t mock_route_ch_start(void *ctx) {
    (void)ctx;
    return HU_OK;
}

static void mock_route_ch_stop(void *ctx) {
    (void)ctx;
}

static const char *mock_route_ch_name(void *ctx) {
    mock_route_channel_ctx_t *m = (mock_route_channel_ctx_t *)ctx;
    return m->channel_name;
}

static bool mock_route_ch_health(void *ctx) {
    (void)ctx;
    return true;
}

static hu_error_t mock_route_ch_send(void *ctx, const char *target, size_t target_len,
                                     const char *message, size_t message_len,
                                     const char *const *media, size_t media_count) {
    (void)media;
    (void)media_count;
    mock_route_channel_ctx_t *m = (mock_route_channel_ctx_t *)ctx;
    m->send_count++;
    m->last_target_len = target_len < sizeof(m->last_target) - 1 ? target_len : sizeof(m->last_target) - 1;
    memcpy(m->last_target, target, m->last_target_len);
    m->last_target[m->last_target_len] = '\0';
    m->last_message_len =
        message_len < sizeof(m->last_message) - 1 ? message_len : sizeof(m->last_message) - 1;
    memcpy(m->last_message, message, m->last_message_len);
    m->last_message[m->last_message_len] = '\0';
    return HU_OK;
}

static const hu_channel_vtable_t mock_route_channel_vtable = {
    .start = mock_route_ch_start,
    .stop = mock_route_ch_stop,
    .send = mock_route_ch_send,
    .name = mock_route_ch_name,
    .health_check = mock_route_ch_health,
};

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

/* Cross-channel style routing: inbound handled on channel_a; outbound directed to channel_b. */
static void test_dispatch_routed_send_delivers_only_to_named_subchannel(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_route_channel_ctx_t ctx_a = {.channel_name = "channel_a"};
    mock_route_channel_ctx_t ctx_b = {.channel_name = "channel_b"};
    hu_channel_t sub_a = {.ctx = &ctx_a, .vtable = &mock_route_channel_vtable};
    hu_channel_t sub_b = {.ctx = &ctx_b, .vtable = &mock_route_channel_vtable};

    hu_channel_t dispatch_ch = {0};
    HU_ASSERT_EQ(hu_dispatch_create(&alloc, &dispatch_ch), HU_OK);
    HU_ASSERT_EQ(hu_dispatch_add_channel(&dispatch_ch, &sub_a), HU_OK);
    HU_ASSERT_EQ(hu_dispatch_add_channel(&dispatch_ch, &sub_b), HU_OK);

    const char *routed_target = "hu:ch:channel_b:user_session_1";
    const char *body = "daemon reply on alternate channel";
    hu_error_t err = dispatch_ch.vtable->send(dispatch_ch.ctx, routed_target, strlen(routed_target),
                                              body, strlen(body), NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ctx_a.send_count, 0u);
    HU_ASSERT_EQ(ctx_b.send_count, 1u);
    HU_ASSERT_STR_EQ(ctx_b.last_target, "user_session_1");
    HU_ASSERT_STR_EQ(ctx_b.last_message, body);

    hu_dispatch_destroy(&dispatch_ch);
}

static void test_dispatch_routed_send_missing_subchannel_returns_not_found(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_route_channel_ctx_t ctx_a = {.channel_name = "channel_a"};
    hu_channel_t sub_a = {.ctx = &ctx_a, .vtable = &mock_route_channel_vtable};

    hu_channel_t dispatch_ch = {0};
    HU_ASSERT_EQ(hu_dispatch_create(&alloc, &dispatch_ch), HU_OK);
    HU_ASSERT_EQ(hu_dispatch_add_channel(&dispatch_ch, &sub_a), HU_OK);

    const char *routed_target = "hu:ch:channel_z:nobody";
    hu_error_t err = dispatch_ch.vtable->send(dispatch_ch.ctx, routed_target, strlen(routed_target),
                                              "x", 1, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
    HU_ASSERT_EQ(ctx_a.send_count, 0u);

    hu_dispatch_destroy(&dispatch_ch);
}

static void test_dispatch_broadcast_send_hits_all_subchannels(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_route_channel_ctx_t ctx_a = {.channel_name = "channel_a"};
    mock_route_channel_ctx_t ctx_b = {.channel_name = "channel_b"};
    hu_channel_t sub_a = {.ctx = &ctx_a, .vtable = &mock_route_channel_vtable};
    hu_channel_t sub_b = {.ctx = &ctx_b, .vtable = &mock_route_channel_vtable};

    hu_channel_t dispatch_ch = {0};
    HU_ASSERT_EQ(hu_dispatch_create(&alloc, &dispatch_ch), HU_OK);
    HU_ASSERT_EQ(hu_dispatch_add_channel(&dispatch_ch, &sub_a), HU_OK);
    HU_ASSERT_EQ(hu_dispatch_add_channel(&dispatch_ch, &sub_b), HU_OK);

    const char *t = "same_target";
    const char *m = "broadcast body";
    hu_error_t err = dispatch_ch.vtable->send(dispatch_ch.ctx, t, strlen(t), m, strlen(m), NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ctx_a.send_count, 1u);
    HU_ASSERT_EQ(ctx_b.send_count, 1u);
    HU_ASSERT_STR_EQ(ctx_a.last_target, t);
    HU_ASSERT_STR_EQ(ctx_b.last_target, t);

    hu_dispatch_destroy(&dispatch_ch);
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
    HU_RUN_TEST(test_dispatch_routed_send_delivers_only_to_named_subchannel);
    HU_RUN_TEST(test_dispatch_routed_send_missing_subchannel_returns_not_found);
    HU_RUN_TEST(test_dispatch_broadcast_send_hits_all_subchannels);
}

#else

void run_dispatch_tests(void) {
    HU_TEST_SUITE("dispatch");
    /* No tests when HU_HAS_DISPATCH is off */
}

#endif
