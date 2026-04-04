#include "test_framework.h"
#include "human/webhook.h"
#include "human/tools/webhook_tools.h"
#include "human/channels/webhook.h"
#include "human/channel.h"
#include "human/core/allocator.h"
#include "human/tool.h"
#include <string.h>

static hu_allocator_t alloc;

static void setup_test(void) {
    alloc = hu_system_allocator();
}

static void cleanup_test(void) {
}

/* ──────────────────────────────────────────────────────────────────────────
 * Webhook Manager Tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_webhook_manager_create_destroy(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_error_t err = hu_webhook_manager_create(&alloc, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_register_single(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *webhook_id = NULL;
    hu_error_t err = hu_webhook_register(&alloc, mgr, "/webhook/test", &webhook_id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(webhook_id);
    HU_ASSERT_STR_CONTAINS(webhook_id, "webhook_");

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_register_multiple(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *id1 = NULL, *id2 = NULL, *id3 = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test1", &id1);
    hu_webhook_register(&alloc, mgr, "/webhook/test2", &id2);
    hu_webhook_register(&alloc, mgr, "/webhook/test3", &id3);

    HU_ASSERT_NOT_NULL(id1);
    HU_ASSERT_NOT_NULL(id2);
    HU_ASSERT_NOT_NULL(id3);
    HU_ASSERT_NEQ(strcmp(id1, id2), 0);
    HU_ASSERT_NEQ(strcmp(id2, id3), 0);

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_list(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *id1 = NULL, *id2 = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test1", &id1);
    hu_webhook_register(&alloc, mgr, "/webhook/test2", &id2);

    hu_webhook_t *webhooks = NULL;
    size_t count = 0;
    hu_error_t err = hu_webhook_list(&alloc, mgr, &webhooks, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2);
    HU_ASSERT_NOT_NULL(webhooks);
    HU_ASSERT_STR_EQ(webhooks[0].path, "/webhook/test1");
    HU_ASSERT_STR_EQ(webhooks[1].path, "/webhook/test2");

    hu_webhook_free_webhooks(&alloc, webhooks, count);
    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_receive_event(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *webhook_id = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test", &webhook_id);

    hu_error_t err = hu_webhook_receive_event(&alloc, mgr, webhook_id, "{\"key\": \"value\"}");
    HU_ASSERT_EQ(err, HU_OK);

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_poll_empty(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *webhook_id = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test", &webhook_id);

    hu_webhook_event_t *events = NULL;
    size_t count = 0;
    hu_error_t err = hu_webhook_poll(&alloc, mgr, webhook_id, &events, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0);
    HU_ASSERT_NULL(events);

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_poll_events(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *webhook_id = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test", &webhook_id);

    hu_webhook_receive_event(&alloc, mgr, webhook_id, "{\"event\": 1}");
    hu_webhook_receive_event(&alloc, mgr, webhook_id, "{\"event\": 2}");
    hu_webhook_receive_event(&alloc, mgr, webhook_id, "{\"event\": 3}");

    hu_webhook_event_t *events = NULL;
    size_t count = 0;
    hu_error_t err = hu_webhook_poll(&alloc, mgr, webhook_id, &events, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 3);
    HU_ASSERT_NOT_NULL(events);
    HU_ASSERT_STR_EQ(events[0].event_data, "{\"event\": 1}");
    HU_ASSERT_STR_EQ(events[1].event_data, "{\"event\": 2}");
    HU_ASSERT_STR_EQ(events[2].event_data, "{\"event\": 3}");

    hu_webhook_free_events(&alloc, events, count);
    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_poll_clears_events(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *webhook_id = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test", &webhook_id);

    hu_webhook_receive_event(&alloc, mgr, webhook_id, "event1");

    hu_webhook_event_t *events = NULL;
    size_t count = 0;
    hu_webhook_poll(&alloc, mgr, webhook_id, &events, &count);
    HU_ASSERT_EQ(count, 1);
    hu_webhook_free_events(&alloc, events, count);

    events = NULL;
    count = 0;
    hu_webhook_poll(&alloc, mgr, webhook_id, &events, &count);
    HU_ASSERT_EQ(count, 0);
    HU_ASSERT_NULL(events);

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_unregister(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *id1 = NULL, *id2 = NULL;
    hu_webhook_register(&alloc, mgr, "/webhook/test1", &id1);
    hu_webhook_register(&alloc, mgr, "/webhook/test2", &id2);

    hu_webhook_t *webhooks = NULL;
    size_t count = 0;
    hu_webhook_list(&alloc, mgr, &webhooks, &count);
    HU_ASSERT_EQ(count, 2);
    hu_webhook_free_webhooks(&alloc, webhooks, count);

    hu_error_t err = hu_webhook_unregister(&alloc, mgr, id1);
    HU_ASSERT_EQ(err, HU_OK);

    webhooks = NULL;
    count = 0;
    hu_webhook_list(&alloc, mgr, &webhooks, &count);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_STR_EQ(webhooks[0].path, "/webhook/test2");
    hu_webhook_free_webhooks(&alloc, webhooks, count);

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_unregister_nonexistent(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    hu_error_t err = hu_webhook_unregister(&alloc, mgr, "nonexistent_id");
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

/* ──────────────────────────────────────────────────────────────────────────
 * Webhook Tool Tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_webhook_register_tool(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    hu_tool_t tool;
    hu_webhook_register_tool_create(&alloc, mgr, &tool);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "webhook_register");

    hu_tool_result_t result;
    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "path", hu_json_string_new(&alloc, "/test", 5));

    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "webhook_");
    HU_ASSERT_STR_CONTAINS(result.output, "id");

    hu_json_free(&alloc, args);
    hu_tool_result_free(&alloc, &result);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_list_tool(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    hu_tool_t tool;
    hu_webhook_list_tool_create(&alloc, mgr, &tool);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "webhook_list");

    hu_tool_result_t result;
    hu_json_value_t *args = hu_json_object_new(&alloc);

    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "webhooks");

    hu_json_free(&alloc, args);
    hu_tool_result_free(&alloc, &result);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

static void test_webhook_poll_tool(void) {
    setup_test();
    hu_webhook_manager_t *mgr = NULL;
    hu_webhook_manager_create(&alloc, &mgr);

    char *webhook_id = NULL;
    hu_webhook_register(&alloc, mgr, "/test", &webhook_id);
    hu_webhook_receive_event(&alloc, mgr, webhook_id, "{\"test\": true}");

    hu_tool_t tool;
    hu_webhook_poll_tool_create(&alloc, mgr, &tool);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "webhook_poll");

    hu_tool_result_t result;
    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "id", hu_json_string_new(&alloc, webhook_id, strlen(webhook_id)));

    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "events");

    hu_json_free(&alloc, args);
    hu_tool_result_free(&alloc, &result);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
    hu_webhook_manager_destroy(&alloc, mgr);
    cleanup_test();
}

/* ──────────────────────────────────────────────────────────────────────────
 * Webhook channel (outbound + gateway JSON parse)
 * ────────────────────────────────────────────────────────────────────────── */

static void test_webhook_channel_create_valid_config(void) {
    setup_test();
    hu_webhook_channel_config_t cfg = {.name = "webhook/myapp",
                                       .callback_url = "https://example.com/hook",
                                       .secret = "test-secret",
                                       .message_field = NULL,
                                       .sender_field = NULL,
                                       .max_message_len = 512};
    hu_channel_t ch = {0};
    hu_error_t err = hu_webhook_channel_create(&alloc, &cfg, &ch);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable);
    HU_ASSERT_NOT_NULL(ch.ctx);
    HU_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "webhook/myapp");
    HU_ASSERT(ch.vtable->health_check(ch.ctx));
    err = ch.vtable->start(ch.ctx);
    HU_ASSERT_EQ(err, HU_OK);
    err = ch.vtable->send(ch.ctx, "session", 7, "hello", 5, NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
    ch.vtable->stop(ch.ctx);
    hu_webhook_channel_destroy(&ch, &alloc);
    cleanup_test();
}

static void test_webhook_on_message_parses_default_fields(void) {
    setup_test();
    hu_webhook_channel_config_t cfg = {0};
    const char *json = "{\"message\":\"ping\",\"sender\":\"user_a\"}";
    char sender[64];
    char message[128];
    hu_error_t err =
        hu_webhook_on_message(json, strlen(json), &cfg, sender, sizeof(sender), message, sizeof(message));
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(sender, "user_a");
    HU_ASSERT_STR_EQ(message, "ping");
    cleanup_test();
}

static void test_webhook_on_message_custom_field_names(void) {
    setup_test();
    char msg_key[] = "body";
    char snd_key[] = "from";
    hu_webhook_channel_config_t cfg = {.message_field = msg_key, .sender_field = snd_key};
    const char *json = "{\"from\":\"bot-1\",\"body\":\"status ok\"}";
    char sender[64];
    char message[128];
    hu_error_t err =
        hu_webhook_on_message(json, strlen(json), &cfg, sender, sizeof(sender), message, sizeof(message));
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(sender, "bot-1");
    HU_ASSERT_STR_EQ(message, "status ok");
    cleanup_test();
}

void run_webhook_tests(void) {
    HU_TEST_SUITE("webhook");

    HU_RUN_TEST(test_webhook_channel_create_valid_config);
    HU_RUN_TEST(test_webhook_on_message_parses_default_fields);
    HU_RUN_TEST(test_webhook_on_message_custom_field_names);
    HU_RUN_TEST(test_webhook_manager_create_destroy);
    HU_RUN_TEST(test_webhook_register_single);
    HU_RUN_TEST(test_webhook_register_multiple);
    HU_RUN_TEST(test_webhook_list);
    HU_RUN_TEST(test_webhook_receive_event);
    HU_RUN_TEST(test_webhook_poll_empty);
    HU_RUN_TEST(test_webhook_poll_events);
    HU_RUN_TEST(test_webhook_poll_clears_events);
    HU_RUN_TEST(test_webhook_unregister);
    HU_RUN_TEST(test_webhook_unregister_nonexistent);
    HU_RUN_TEST(test_webhook_register_tool);
    HU_RUN_TEST(test_webhook_list_tool);
    HU_RUN_TEST(test_webhook_poll_tool);
}
