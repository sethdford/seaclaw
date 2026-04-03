/*
 * Tests for send_voice_message tool and pending voice state.
 */
#include "human/agent.h"
#include "human/agent/tool_context.h"
#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/tools/send_voice_message.h"
#include "test_framework.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static hu_allocator_t test_alloc;

static void setup_alloc(void) {
    test_alloc = hu_system_allocator();
}

/* ── Pending voice thread-local state ──────────────────────────────────── */

static void test_pending_voice_initially_false(void) {
    hu_agent_clear_pending_voice();
    HU_ASSERT_FALSE(hu_agent_has_pending_voice());
    HU_ASSERT_NULL(hu_agent_pending_voice_emotion());
    size_t len = 99;
    HU_ASSERT_NULL(hu_agent_pending_voice_transcript(&len));
    HU_ASSERT_EQ(len, (size_t)0);
}

static void test_pending_voice_request_sets_flag(void) {
    hu_agent_clear_pending_voice();
    hu_agent_request_voice_send(NULL, NULL, 0);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_NULL(hu_agent_pending_voice_emotion());
    HU_ASSERT_NULL(hu_agent_pending_voice_transcript(NULL));
    hu_agent_clear_pending_voice();
}

static void test_pending_voice_stores_emotion(void) {
    hu_agent_clear_pending_voice();
    hu_agent_request_voice_send("sympathetic", NULL, 0);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_NOT_NULL(hu_agent_pending_voice_emotion());
    HU_ASSERT_STR_EQ(hu_agent_pending_voice_emotion(), "sympathetic");
    hu_agent_clear_pending_voice();
}

static void test_pending_voice_stores_transcript(void) {
    hu_agent_clear_pending_voice();
    const char *text = "I'm here for you.";
    hu_agent_request_voice_send(NULL, text, strlen(text));
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    size_t len = 0;
    const char *t = hu_agent_pending_voice_transcript(&len);
    HU_ASSERT_NOT_NULL(t);
    HU_ASSERT_EQ(len, strlen(text));
    HU_ASSERT_STR_EQ(t, "I'm here for you.");
    hu_agent_clear_pending_voice();
}

static void test_pending_voice_stores_both(void) {
    hu_agent_clear_pending_voice();
    const char *text = "Take care of yourself.";
    hu_agent_request_voice_send("calm", text, strlen(text));
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_STR_EQ(hu_agent_pending_voice_emotion(), "calm");
    size_t len = 0;
    const char *t = hu_agent_pending_voice_transcript(&len);
    HU_ASSERT_STR_EQ(t, "Take care of yourself.");
    HU_ASSERT_EQ(len, strlen(text));
    hu_agent_clear_pending_voice();
}

static void test_pending_voice_clear_resets_all(void) {
    hu_agent_request_voice_send("excited", "Congrats!", 9);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    hu_agent_clear_pending_voice();
    HU_ASSERT_FALSE(hu_agent_has_pending_voice());
    HU_ASSERT_NULL(hu_agent_pending_voice_emotion());
    size_t len = 99;
    HU_ASSERT_NULL(hu_agent_pending_voice_transcript(&len));
    HU_ASSERT_EQ(len, (size_t)0);
}

static void test_pending_voice_truncates_long_transcript(void) {
    hu_agent_clear_pending_voice();
    char long_text[5000];
    memset(long_text, 'A', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    hu_agent_request_voice_send(NULL, long_text, sizeof(long_text) - 1);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    size_t len = 0;
    const char *t = hu_agent_pending_voice_transcript(&len);
    HU_ASSERT_NOT_NULL(t);
    HU_ASSERT_LT(len, (size_t)5000);
    HU_ASSERT_GT(len, (size_t)0);
    hu_agent_clear_pending_voice();
}

static void test_pending_voice_survives_clear_current(void) {
    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_request_voice_send("calm", "Hello there.", 12);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());

    hu_agent_clear_current_for_tools();
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_STR_EQ(hu_agent_pending_voice_emotion(), "calm");
    size_t len = 0;
    HU_ASSERT_STR_EQ(hu_agent_pending_voice_transcript(&len), "Hello there.");

    hu_agent_clear_pending_voice();
    HU_ASSERT_FALSE(hu_agent_has_pending_voice());
}

/* ── Tool vtable ──────────────────────────────────────────────────────── */

static void test_svm_create_succeeds(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_error_t err = hu_send_voice_message_create(&test_alloc, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "send_voice_message");
    HU_ASSERT_NOT_NULL(tool.vtable->description(tool.ctx));
    HU_ASSERT_NOT_NULL(tool.vtable->parameters_json(tool.ctx));
}

static void test_svm_create_null_out_fails(void) {
    setup_alloc();
    hu_error_t err = hu_send_voice_message_create(&test_alloc, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_svm_execute_no_agent_returns_fail(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_clear_current_for_tools();

    hu_json_value_t args = {.type = HU_JSON_OBJECT};
    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, &args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_STR_CONTAINS(result.error_msg, "agent context");
}

static void test_svm_execute_no_args_queues_voice(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    /* Need an agent context — use a minimal fake */
    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    hu_json_value_t args = {.type = HU_JSON_OBJECT};
    args.data.object.pairs = NULL;
    args.data.object.len = 0;
    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, &args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_NULL(hu_agent_pending_voice_emotion());
    HU_ASSERT_NULL(hu_agent_pending_voice_transcript(NULL));
    HU_ASSERT_STR_CONTAINS(result.output, "requested");
    hu_tool_result_free(&test_alloc, &result);
    hu_agent_clear_current_for_tools();
}

static void test_svm_execute_with_transcript_stores_it(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    const char *json = "{\"transcript\":\"I miss you.\"}";
    hu_json_value_t *parsed = NULL;
    hu_error_t perr = hu_json_parse(&test_alloc, json, strlen(json), &parsed);
    HU_ASSERT_EQ(perr, HU_OK);
    HU_ASSERT_NOT_NULL(parsed);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, parsed, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());

    size_t tlen = 0;
    const char *t = hu_agent_pending_voice_transcript(&tlen);
    HU_ASSERT_NOT_NULL(t);
    HU_ASSERT_STR_EQ(t, "I miss you.");
    HU_ASSERT_EQ(tlen, (size_t)11);
    HU_ASSERT_STR_CONTAINS(result.output, "custom transcript");
    HU_ASSERT_STR_CONTAINS(result.output, "requested");

    hu_tool_result_free(&test_alloc, &result);
    hu_json_free(&test_alloc, parsed);
    hu_agent_clear_current_for_tools();
}

static void test_svm_execute_with_emotion_stores_it(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    const char *json = "{\"emotion\":\"sympathetic\"}";
    hu_json_value_t *parsed = NULL;
    hu_json_parse(&test_alloc, json, strlen(json), &parsed);

    hu_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &test_alloc, parsed, &result);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_STR_EQ(hu_agent_pending_voice_emotion(), "sympathetic");

    hu_tool_result_free(&test_alloc, &result);
    hu_json_free(&test_alloc, parsed);
    hu_agent_clear_current_for_tools();
}

static void test_svm_execute_with_both_params(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    const char *json = "{\"transcript\":\"You've got this.\",\"emotion\":\"calm\"}";
    hu_json_value_t *parsed = NULL;
    hu_json_parse(&test_alloc, json, strlen(json), &parsed);

    hu_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &test_alloc, parsed, &result);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    HU_ASSERT_STR_EQ(hu_agent_pending_voice_emotion(), "calm");
    size_t tlen = 0;
    const char *t = hu_agent_pending_voice_transcript(&tlen);
    HU_ASSERT_STR_EQ(t, "You've got this.");
    HU_ASSERT_EQ(tlen, (size_t)16);

    hu_tool_result_free(&test_alloc, &result);
    hu_json_free(&test_alloc, parsed);
    hu_agent_clear_current_for_tools();
}

static void test_svm_execute_invalid_emotion_rejected(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    const char *json = "{\"emotion\":\"super_fake_emotion\"}";
    hu_json_value_t *parsed = NULL;
    hu_json_parse(&test_alloc, json, strlen(json), &parsed);

    hu_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &test_alloc, parsed, &result);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_STR_CONTAINS(result.error_msg, "emotion");
    HU_ASSERT_FALSE(hu_agent_has_pending_voice());

    hu_json_free(&test_alloc, parsed);
    hu_agent_clear_current_for_tools();
}

static void test_svm_execute_valid_emotions_accepted(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);
    hu_agent_t fake_agent = {0};

    static const char *valid[] = {"content", "calm", "excited", "sympathetic",
                                  "sad", "neutral", "contemplative", "joking/comedic"};
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        hu_agent_set_current_for_tools(&fake_agent);
        hu_agent_clear_pending_voice();

        char json[128];
        snprintf(json, sizeof(json), "{\"emotion\":\"%s\"}", valid[i]);
        hu_json_value_t *parsed = NULL;
        hu_json_parse(&test_alloc, json, strlen(json), &parsed);

        hu_tool_result_t result = {0};
        tool.vtable->execute(tool.ctx, &test_alloc, parsed, &result);
        HU_ASSERT_TRUE(result.success);
        HU_ASSERT_TRUE(hu_agent_has_pending_voice());
        HU_ASSERT_STR_EQ(hu_agent_pending_voice_emotion(), valid[i]);

        hu_tool_result_free(&test_alloc, &result);
        hu_json_free(&test_alloc, parsed);
        hu_agent_clear_current_for_tools();
    }
    hu_agent_clear_pending_voice();
}

static void test_svm_execute_double_call_rejected(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    hu_json_value_t args = {.type = HU_JSON_OBJECT};
    args.data.object.pairs = NULL;
    args.data.object.len = 0;

    hu_tool_result_t r1 = {0};
    tool.vtable->execute(tool.ctx, &test_alloc, &args, &r1);
    HU_ASSERT_TRUE(r1.success);
    HU_ASSERT_TRUE(hu_agent_has_pending_voice());
    hu_tool_result_free(&test_alloc, &r1);

    hu_tool_result_t r2 = {0};
    tool.vtable->execute(tool.ctx, &test_alloc, &args, &r2);
    HU_ASSERT_FALSE(r2.success);
    HU_ASSERT_STR_CONTAINS(r2.error_msg, "already requested");

    hu_agent_clear_current_for_tools();
    hu_agent_clear_pending_voice();
}

static void test_svm_execute_utf8_truncation_preserves_chars(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);

    hu_agent_t fake_agent = {0};
    hu_agent_set_current_for_tools(&fake_agent);
    hu_agent_clear_pending_voice();

    /* Build a string of 2-byte UTF-8 chars (e.g. 'é' = 0xC3 0xA9) padded to >4000 bytes */
    char long_utf8[4100];
    size_t pos = 0;
    while (pos + 2 <= sizeof(long_utf8) - 1) {
        long_utf8[pos++] = (char)0xC3;
        long_utf8[pos++] = (char)0xA9;
    }
    long_utf8[pos] = '\0';

    /* Transcript > 4000 bytes should be truncated at a char boundary */
    char json_buf[8300];
    snprintf(json_buf, sizeof(json_buf), "{\"transcript\":\"%s\"}", long_utf8);
    hu_json_value_t *parsed = NULL;
    hu_json_parse(&test_alloc, json_buf, strlen(json_buf), &parsed);

    hu_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &test_alloc, parsed, &result);
    HU_ASSERT_TRUE(result.success);

    size_t tlen = 0;
    const char *t = hu_agent_pending_voice_transcript(&tlen);
    HU_ASSERT_NOT_NULL(t);
    HU_ASSERT_TRUE(tlen <= 4000);
    HU_ASSERT_TRUE(tlen % 2 == 0);

    hu_tool_result_free(&test_alloc, &result);
    hu_json_free(&test_alloc, parsed);
    hu_agent_clear_current_for_tools();
    hu_agent_clear_pending_voice();
}

static void test_svm_execute_null_out_returns_error(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);
    hu_json_value_t args = {.type = HU_JSON_OBJECT};
    hu_error_t err = tool.vtable->execute(tool.ctx, &test_alloc, &args, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_svm_params_json_is_valid(void) {
    setup_alloc();
    hu_tool_t tool = {0};
    hu_send_voice_message_create(&test_alloc, &tool);
    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    hu_json_value_t *parsed = NULL;
    hu_error_t err = hu_json_parse(&test_alloc, params, strlen(params), &parsed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    HU_ASSERT_EQ(parsed->type, HU_JSON_OBJECT);
    hu_json_free(&test_alloc, parsed);
}

/* ── Registration ─────────────────────────────────────────────────────── */

void run_send_voice_message_tests(void) {
    HU_TEST_SUITE("Send voice message");

    HU_RUN_TEST(test_pending_voice_initially_false);
    HU_RUN_TEST(test_pending_voice_request_sets_flag);
    HU_RUN_TEST(test_pending_voice_stores_emotion);
    HU_RUN_TEST(test_pending_voice_stores_transcript);
    HU_RUN_TEST(test_pending_voice_stores_both);
    HU_RUN_TEST(test_pending_voice_clear_resets_all);
    HU_RUN_TEST(test_pending_voice_truncates_long_transcript);
    HU_RUN_TEST(test_pending_voice_survives_clear_current);

    HU_RUN_TEST(test_svm_create_succeeds);
    HU_RUN_TEST(test_svm_create_null_out_fails);
    HU_RUN_TEST(test_svm_execute_no_agent_returns_fail);
    HU_RUN_TEST(test_svm_execute_no_args_queues_voice);
    HU_RUN_TEST(test_svm_execute_with_transcript_stores_it);
    HU_RUN_TEST(test_svm_execute_with_emotion_stores_it);
    HU_RUN_TEST(test_svm_execute_with_both_params);
    HU_RUN_TEST(test_svm_execute_invalid_emotion_rejected);
    HU_RUN_TEST(test_svm_execute_valid_emotions_accepted);
    HU_RUN_TEST(test_svm_execute_double_call_rejected);
    HU_RUN_TEST(test_svm_execute_utf8_truncation_preserves_chars);
    HU_RUN_TEST(test_svm_execute_null_out_returns_error);
    HU_RUN_TEST(test_svm_params_json_is_valid);
}
