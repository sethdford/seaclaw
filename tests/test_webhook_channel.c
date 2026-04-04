#include "test_framework.h"
#include "human/channels/webhook.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Create / Destroy ────────────────────────────────────────────── */

static void webhook_channel_create_null_alloc_fails(void) {
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {.name = "test"};
    HU_ASSERT(hu_webhook_channel_create(NULL, &cfg, &ch) == HU_ERR_INVALID_ARGUMENT);
}

static void webhook_channel_create_null_cfg_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    HU_ASSERT(hu_webhook_channel_create(&alloc, NULL, &ch) == HU_ERR_INVALID_ARGUMENT);
}

static void webhook_channel_create_null_out_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webhook_channel_config_t cfg = {.name = "test"};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void webhook_channel_create_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {
        .name = "my-hook",
        .callback_url = "https://example.com/hook",
        .secret = "s3cret",
        .message_field = "text",
        .sender_field = "from",
    };
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(ch.vtable != NULL);
    HU_ASSERT(ch.ctx != NULL);
    hu_webhook_channel_destroy(&ch, &alloc);
}

static void webhook_channel_destroy_null_safe(void) {
    hu_webhook_channel_destroy(NULL, NULL);
    hu_channel_t ch = {0};
    hu_webhook_channel_destroy(&ch, NULL);
}

/* ── Vtable name ─────────────────────────────────────────────────── */

static void webhook_channel_name_custom(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {.name = "alerts"};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(strcmp(ch.vtable->name(ch.ctx), "alerts") == 0);
    hu_webhook_channel_destroy(&ch, &alloc);
}

static void webhook_channel_name_default(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {0};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(strcmp(ch.vtable->name(ch.ctx), "webhook") == 0);
    hu_webhook_channel_destroy(&ch, &alloc);
}

/* ── Health check ────────────────────────────────────────────────── */

static void webhook_channel_health_check_true(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {.name = "hc"};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(ch.vtable->health_check(ch.ctx));
    hu_webhook_channel_destroy(&ch, &alloc);
}

/* ── Start / Stop lifecycle ──────────────────────────────────────── */

static void webhook_channel_start_stop_cycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {.name = "life"};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(ch.vtable->start(ch.ctx) == HU_OK);
    ch.vtable->stop(ch.ctx);
    hu_webhook_channel_destroy(&ch, &alloc);
}

/* ── Send (test mode — no-op) ────────────────────────────────────── */

static void webhook_channel_send_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {.name = "snd", .callback_url = "https://example.com/h"};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(ch.vtable->start(ch.ctx) == HU_OK);
    HU_ASSERT(ch.vtable->send(ch.ctx, "user1", 5, "hello", 5, NULL, 0) == HU_OK);
    ch.vtable->stop(ch.ctx);
    hu_webhook_channel_destroy(&ch, &alloc);
}

static void webhook_channel_send_null_ctx_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_webhook_channel_config_t cfg = {.name = "snd"};
    HU_ASSERT(hu_webhook_channel_create(&alloc, &cfg, &ch) == HU_OK);
    HU_ASSERT(ch.vtable->send(NULL, "u", 1, "x", 1, NULL, 0) == HU_ERR_INVALID_ARGUMENT);
    hu_webhook_channel_destroy(&ch, &alloc);
}

/* ── hu_webhook_on_message ───────────────────────────────────────── */

static void webhook_on_message_parses_default_fields(void) {
    hu_webhook_channel_config_t cfg = {0};
    char sender[64], msg[256];
    const char *body = "{\"sender\":\"alice\",\"message\":\"hi there\"}";
    HU_ASSERT(hu_webhook_on_message(body, strlen(body), &cfg,
                                     sender, sizeof(sender), msg, sizeof(msg)) == HU_OK);
    HU_ASSERT(strcmp(sender, "alice") == 0);
    HU_ASSERT(strcmp(msg, "hi there") == 0);
}

static void webhook_on_message_custom_fields(void) {
    hu_webhook_channel_config_t cfg = {.message_field = "text", .sender_field = "from"};
    char sender[64], msg[256];
    const char *body = "{\"from\":\"bob\",\"text\":\"hey\"}";
    HU_ASSERT(hu_webhook_on_message(body, strlen(body), &cfg,
                                     sender, sizeof(sender), msg, sizeof(msg)) == HU_OK);
    HU_ASSERT(strcmp(sender, "bob") == 0);
    HU_ASSERT(strcmp(msg, "hey") == 0);
}

static void webhook_on_message_null_cfg_fails(void) {
    char s[64], m[256];
    HU_ASSERT(hu_webhook_on_message("{}", 2, NULL, s, sizeof(s), m, sizeof(m)) == HU_ERR_INVALID_ARGUMENT);
}

static void webhook_on_message_null_sender_out_fails(void) {
    hu_webhook_channel_config_t cfg = {0};
    char m[256];
    HU_ASSERT(hu_webhook_on_message("{}", 2, &cfg, NULL, 0, m, sizeof(m)) == HU_ERR_INVALID_ARGUMENT);
}

static void webhook_on_message_invalid_json(void) {
    hu_webhook_channel_config_t cfg = {0};
    char s[64], m[256];
    HU_ASSERT(hu_webhook_on_message("not json", 8, &cfg, s, sizeof(s), m, sizeof(m)) != HU_OK);
}

static void webhook_on_message_non_object_json(void) {
    hu_webhook_channel_config_t cfg = {0};
    char s[64], m[256];
    HU_ASSERT(hu_webhook_on_message("[1,2]", 5, &cfg, s, sizeof(s), m, sizeof(m)) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Suite registration ──────────────────────────────────────────── */

void run_webhook_channel_tests(void) {
    HU_TEST_SUITE("webhook_channel");

    HU_RUN_TEST(webhook_channel_create_null_alloc_fails);
    HU_RUN_TEST(webhook_channel_create_null_cfg_fails);
    HU_RUN_TEST(webhook_channel_create_null_out_fails);
    HU_RUN_TEST(webhook_channel_create_succeeds);
    HU_RUN_TEST(webhook_channel_destroy_null_safe);

    HU_RUN_TEST(webhook_channel_name_custom);
    HU_RUN_TEST(webhook_channel_name_default);
    HU_RUN_TEST(webhook_channel_health_check_true);
    HU_RUN_TEST(webhook_channel_start_stop_cycle);

    HU_RUN_TEST(webhook_channel_send_test_mode);
    HU_RUN_TEST(webhook_channel_send_null_ctx_fails);

    HU_RUN_TEST(webhook_on_message_parses_default_fields);
    HU_RUN_TEST(webhook_on_message_custom_fields);
    HU_RUN_TEST(webhook_on_message_null_cfg_fails);
    HU_RUN_TEST(webhook_on_message_null_sender_out_fails);
    HU_RUN_TEST(webhook_on_message_invalid_json);
    HU_RUN_TEST(webhook_on_message_non_object_json);
}
