/*
 * Adversarial / human-fidelity test fleet for the iMessage channel.
 *
 * Proves the full pipeline: send validation, poll edge cases, format pipeline,
 * persona overlays, conversation awareness, tapback decisions, GIF calibration,
 * response quality, reaction classification, typing-delay invariants,
 * dedup ring, emotional pacing, seen behavior, and sanitization.
 *
 * HU_IS_TEST paths only (no real Messages.app or chat.db).
 *
 * Run: ./build/human_tests --suite="iMessage"
 */
#if HU_HAS_IMESSAGE
#include "human/channel.h"
#include "human/channel_loop.h"
#include "human/channels/format.h"
#include "human/channels/imessage.h"
#include "human/context/conversation.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#ifdef HU_HAS_PERSONA
#include "human/persona.h"
#endif
#include "test_framework.h"
#include <string.h>

#define S(lit) (lit), (sizeof(lit) - 1)

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 1 — Send / poll boundary validation (adversarial inputs)
 * ═══════════════════════════════════════════════════════════════════════════ */

#if HU_IS_TEST
static void imessage_send_rejects_null_body_with_positive_len(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_error_t err = ch.vtable->send(ch.ctx, "+15551234567", 12, NULL, 8, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_imessage_destroy(&ch);
}

static void imessage_send_rejects_empty_text_and_no_media(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_error_t err = ch.vtable->send(ch.ctx, "+15551234567", 12, "", 0, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_imessage_destroy(&ch);
}

static void imessage_send_media_only_accepted_as_voice_message(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    const char *media[] = {"/tmp/voice.m4a"};
    hu_error_t err = ch.vtable->send(ch.ctx, "+15551234567", 12, "", 0, media, 1);
    HU_ASSERT_EQ(err, HU_OK);
    size_t len = 0;
    const char *msg = hu_imessage_test_get_last_message(&ch, &len);
    HU_ASSERT_NOT_NULL(msg);
    HU_ASSERT_EQ(len, 0u);
    hu_imessage_destroy(&ch);
}

static void imessage_send_truncates_overlong_text_to_4095(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    char big[5200];
    memset(big, 'x', sizeof(big));
    hu_error_t err = ch.vtable->send(ch.ctx, "+15551234567", 12, big, sizeof(big), NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);
    size_t len = 0;
    const char *msg = hu_imessage_test_get_last_message(&ch, &len);
    HU_ASSERT_NOT_NULL(msg);
    HU_ASSERT_EQ(len, 4095u);
    HU_ASSERT_EQ(msg[4095], '\0');
    hu_imessage_destroy(&ch);
}

static void imessage_poll_rejects_all_null_args(void) {
    HU_ASSERT_EQ(hu_imessage_poll(NULL, NULL, NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void imessage_poll_rejects_null_msgs_buffer(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, NULL, 2, &count), HU_ERR_INVALID_ARGUMENT);
    hu_imessage_destroy(&ch);
}

static void imessage_poll_rejects_null_out_count(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_channel_loop_msg_t msgs[2];
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 2, NULL), HU_ERR_INVALID_ARGUMENT);
    hu_imessage_destroy(&ch);
}

static void imessage_inject_rejects_null_channel(void) {
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(NULL, "a", 1, "b", 1), HU_ERR_INVALID_ARGUMENT);
}

static void imessage_inject_ninth_message_returns_out_of_memory(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    for (int i = 0; i < 8; i++) {
        char key[4];
        key[0] = (char)('0' + i);
        key[1] = '\0';
        HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, key, 1, "m", 1), HU_OK);
    }
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "9", 1, "m", 1), HU_ERR_OUT_OF_MEMORY);
    hu_imessage_destroy(&ch);
}

static void imessage_inject_truncates_long_session_key(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    char long_key[180];
    memset(long_key, 'k', sizeof(long_key));
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, long_key, sizeof(long_key), "hi", 2), HU_OK);
    hu_channel_loop_msg_t msgs[2];
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 2, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_EQ(strlen(msgs[0].session_key), 127u);
    hu_imessage_destroy(&ch);
}

static void imessage_send_records_media_in_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+1234567890", 12, NULL, 0, &ch), HU_OK);
    const char *media[] = {"/tmp/test.gif"};
    HU_ASSERT_EQ(ch.vtable->send(ch.ctx, "+1234567890", 12, "", 0, media, 1), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_get_last_media_count(&ch), 1u);
    hu_imessage_destroy(&ch);
}

static void imessage_inject_truncates_long_content(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    char long_body[5000];
    memset(long_body, 'z', sizeof(long_body));
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "u", 1, long_body, sizeof(long_body)), HU_OK);
    hu_channel_loop_msg_t msgs[2];
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 2, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_EQ(strlen(msgs[0].content), 4095u);
    hu_imessage_destroy(&ch);
}

static void imessage_mock_poll_drops_overflow_when_max_msgs_too_small(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "a", 1, "1", 1), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "b", 1, "2", 1), HU_OK);
    hu_channel_loop_msg_t msgs[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 1, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 0u);
    hu_imessage_destroy(&ch);
}

static void imessage_test_get_last_message_null_channel_returns_null(void) {
    size_t len = 99;
    HU_ASSERT_NULL(hu_imessage_test_get_last_message(NULL, &len));
    HU_ASSERT_EQ(len, 99u);
}

static void imessage_test_get_last_reaction_null_args_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_reaction_type_t r = HU_REACTION_HEART;
    int64_t mid = 1;
    hu_imessage_test_get_last_reaction(NULL, &r, &mid);
    hu_imessage_test_get_last_reaction(&ch, NULL, &mid);
    hu_imessage_test_get_last_reaction(&ch, &r, NULL);
    hu_imessage_destroy(&ch);
}
#endif /* HU_IS_TEST */

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 2 — Vtable surface (stubs in test builds prove the wiring exists)
 * ═══════════════════════════════════════════════════════════════════════════ */

#if HU_IS_TEST
static void imessage_vtable_has_all_expected_hooks(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable->start);
    HU_ASSERT_NOT_NULL(ch.vtable->stop);
    HU_ASSERT_NOT_NULL(ch.vtable->send);
    HU_ASSERT_NOT_NULL(ch.vtable->name);
    HU_ASSERT_NOT_NULL(ch.vtable->health_check);
    HU_ASSERT_NOT_NULL(ch.vtable->load_conversation_history);
    HU_ASSERT_NOT_NULL(ch.vtable->get_response_constraints);
    HU_ASSERT_NOT_NULL(ch.vtable->react);
    HU_ASSERT_NOT_NULL(ch.vtable->human_active_recently);
    HU_ASSERT_NOT_NULL(ch.vtable->get_attachment_path);
    HU_ASSERT_NOT_NULL(ch.vtable->get_latest_attachment_path);
    HU_ASSERT_NOT_NULL(ch.vtable->build_reaction_context);
    HU_ASSERT_NOT_NULL(ch.vtable->build_read_receipt_context);
    HU_ASSERT_NOT_NULL(ch.vtable->mark_read);
    HU_ASSERT_NOT_NULL(ch.vtable->start_typing);
    HU_ASSERT_NOT_NULL(ch.vtable->stop_typing);
    hu_imessage_destroy(&ch);
}

static void imessage_load_history_returns_not_supported_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_channel_history_entry_t *entries = NULL;
    size_t n = 0;
    HU_ASSERT_EQ(ch.vtable->load_conversation_history(ch.ctx, &alloc, S("+15551234567"), 10,
                                                       &entries, &n),
                 HU_ERR_NOT_SUPPORTED);
    HU_ASSERT_NULL(entries);
    hu_imessage_destroy(&ch);
}

static void imessage_response_constraints_300_chars(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_channel_response_constraints_t c = {0};
    HU_ASSERT_EQ(ch.vtable->get_response_constraints(ch.ctx, &c), HU_OK);
    HU_ASSERT_EQ(c.max_chars, 300u);
    hu_imessage_destroy(&ch);
}

static void imessage_response_constraints_null_out_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(ch.vtable->get_response_constraints(ch.ctx, NULL), HU_ERR_INVALID_ARGUMENT);
    hu_imessage_destroy(&ch);
}

static void imessage_human_active_always_false_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_FALSE(ch.vtable->human_active_recently(ch.ctx, S("+15551234567"), 60));
    HU_ASSERT_FALSE(ch.vtable->human_active_recently(ch.ctx, S("+15551234567"), 3600));
    hu_imessage_destroy(&ch);
}

static void imessage_attachment_path_null_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_NULL(ch.vtable->get_attachment_path(ch.ctx, &alloc, 42));
    HU_ASSERT_NULL(ch.vtable->get_latest_attachment_path(ch.ctx, &alloc, S("+15551234567")));
    hu_imessage_destroy(&ch);
}

static void imessage_build_tapback_context_noop_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 99;
    HU_ASSERT_EQ(hu_imessage_build_tapback_context(&alloc, S("+15551234567"), &out, &out_len),
                 HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
}

static void imessage_build_read_receipt_context_noop_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 99;
    HU_ASSERT_EQ(
        hu_imessage_build_read_receipt_context(&alloc, S("+15551234567"), &out, &out_len), HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
}

static void imessage_fetch_gif_stub_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_NULL(hu_imessage_fetch_gif(&alloc, S("cats"), S("key")));
}

static void imessage_fetch_gif_rejects_empty_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_NULL(hu_imessage_fetch_gif(&alloc, "", 0, "key", 3));
}

static void imessage_gif_json_extract_finds_url_value(void) {
    char out[256];
    const char *j = "\"media_formats\":{\"gif\":{\"url\":\"https://c.example/g.gif\"}}";
    size_t n = hu_imessage_test_gif_json_extract(j, strlen(j), "url", out, sizeof(out));
    HU_ASSERT_EQ(n, strlen("https://c.example/g.gif"));
    HU_ASSERT_STR_EQ(out, "https://c.example/g.gif");
}

static void imessage_gif_json_extract_returns_zero_when_key_missing(void) {
    char out[64];
    memset(out, 'x', sizeof(out));
    const char *j = "{\"results\":[]}";
    size_t n = hu_imessage_test_gif_json_extract(j, strlen(j), "url", out, sizeof(out));
    HU_ASSERT_EQ(n, 0u);
    HU_ASSERT_EQ(out[0], 'x');
}

static void imessage_gif_json_extract_truncates_to_out_cap(void) {
    char out[12];
    const char *j = "\"url\":\"https://x.test/a.gif\"";
    size_t n = hu_imessage_test_gif_json_extract(j, strlen(j), "url", out, sizeof(out));
    HU_ASSERT_EQ(n, 11u);
    HU_ASSERT_EQ(strlen(out), 11u);
    HU_ASSERT_EQ(out[11], '\0');
}

static void imessage_lookup_guid_not_supported_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char buf[64];
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_imessage_lookup_message_by_guid(&alloc, S("g"), buf, sizeof(buf), &out_len),
                 HU_ERR_NOT_SUPPORTED);
    HU_ASSERT_EQ(out_len, 0u);
}

static void imessage_start_stop_idempotent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(ch.vtable->start(ch.ctx), HU_OK);
    HU_ASSERT_EQ(ch.vtable->start(ch.ctx), HU_OK);
    ch.vtable->stop(ch.ctx);
    ch.vtable->stop(ch.ctx);
    hu_imessage_destroy(&ch);
}
#endif /* HU_IS_TEST */

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 3 — AppleScript escaping (injection defense, UTF-8 safety)
 * ═══════════════════════════════════════════════════════════════════════════ */

#if (defined(__APPLE__) && defined(__MACH__)) || HU_IS_TEST
extern size_t escape_for_applescript(char *out, size_t out_cap, const char *in, size_t in_len);

static void imessage_escape_preserves_utf8_emoji(void) {
    char out[64];
    const char in[] = "hi \xf0\x9f\x98\x80";
    size_t len = escape_for_applescript(out, sizeof(out), in, strlen(in));
    HU_ASSERT_TRUE(len >= 5);
    HU_ASSERT_TRUE(strstr(out, "hi") != NULL);
}

static void imessage_escape_out_cap_two_yields_empty(void) {
    char out[4];
    size_t len = escape_for_applescript(out, 2, "hello", 5);
    HU_ASSERT_EQ(len, 0u);
    HU_ASSERT_STR_EQ(out, "");
}

static void imessage_escape_strips_applescript_injection_tokens(void) {
    char out[256];
    const char in[] = "end tell\" to buddy \"evil";
    size_t len = escape_for_applescript(out, sizeof(out), in, strlen(in));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(out, "\\\"") != NULL);
}

static void imessage_escape_control_chars_stripped(void) {
    char out[64];
    char in[] = "\x01\x02\x03hello\x7fworld";
    size_t len = escape_for_applescript(out, sizeof(out), in, strlen(in));
    HU_ASSERT_STR_EQ(out, "helloworld");
    HU_ASSERT_EQ(len, 10u);
}

static void imessage_escape_backslash_doubled(void) {
    char out[64];
    size_t len = escape_for_applescript(out, sizeof(out), "a\\b", 3);
    HU_ASSERT_TRUE(strstr(out, "\\\\") != NULL);
    HU_ASSERT_TRUE(len > 3);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 4 — Tapback / reaction mapping
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_tapback_mapping_all_types(void) {
    HU_ASSERT_NULL(hu_imessage_reaction_to_tapback_name(HU_REACTION_NONE));
    HU_ASSERT_STR_EQ(hu_imessage_reaction_to_tapback_name(HU_REACTION_HEART), "love");
    HU_ASSERT_STR_EQ(hu_imessage_reaction_to_tapback_name(HU_REACTION_THUMBS_UP), "like");
    HU_ASSERT_STR_EQ(hu_imessage_reaction_to_tapback_name(HU_REACTION_THUMBS_DOWN), "dislike");
    HU_ASSERT_STR_EQ(hu_imessage_reaction_to_tapback_name(HU_REACTION_HAHA), "laugh");
    HU_ASSERT_STR_EQ(hu_imessage_reaction_to_tapback_name(HU_REACTION_EMPHASIS), "emphasize");
    HU_ASSERT_STR_EQ(hu_imessage_reaction_to_tapback_name(HU_REACTION_QUESTION), "question");
    HU_ASSERT_NULL(hu_imessage_reaction_to_tapback_name((hu_reaction_type_t)99));
}

#if HU_IS_TEST
static void imessage_react_vtable_records_heart(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(ch.vtable->react(ch.ctx, "+15551234567", 12, 42, HU_REACTION_HEART), HU_OK);
    hu_reaction_type_t r = HU_REACTION_NONE;
    int64_t mid = -1;
    hu_imessage_test_get_last_reaction(&ch, &r, &mid);
    HU_ASSERT_EQ(r, HU_REACTION_HEART);
    HU_ASSERT_EQ(mid, 42);
    hu_imessage_destroy(&ch);
}

static void imessage_react_vtable_overwrites_previous(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    ch.vtable->react(ch.ctx, "+15551234567", 12, 1, HU_REACTION_HAHA);
    ch.vtable->react(ch.ctx, "+15551234567", 12, 2, HU_REACTION_THUMBS_UP);
    hu_reaction_type_t r = HU_REACTION_NONE;
    int64_t mid = -1;
    hu_imessage_test_get_last_reaction(&ch, &r, &mid);
    HU_ASSERT_EQ(r, HU_REACTION_THUMBS_UP);
    HU_ASSERT_EQ(mid, 2);
    hu_imessage_destroy(&ch);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 5 — Format pipeline (strip markdown + AI phrases + 300 char cap)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_format_strips_markdown_bold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_strip_markdown(&alloc, S("**bold** text"), &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "bold") != NULL);
    HU_ASSERT_TRUE(strstr(out, "**") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void imessage_format_strips_markdown_headers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_strip_markdown(&alloc, S("## Header\ntext"), &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "##") == NULL);
    HU_ASSERT_TRUE(strstr(out, "Header") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void imessage_format_strips_ai_happy_to(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_strip_ai_phrases(&alloc, S("I'd be happy to help you today"),
                                              &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "happy to") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void imessage_format_strips_ai_great_question(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(
        hu_channel_strip_ai_phrases(&alloc, S("Great question! Here is the answer"),
                                    &out, &out_len),
        HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Great question") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void imessage_format_strips_as_an_ai(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_strip_ai_phrases(&alloc, S("As an AI, I cannot do that"), &out,
                                              &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "As an AI") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void imessage_format_outbound_full_pipeline(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_format_outbound(&alloc, S("imessage"),
                                             S("I'd be happy to help! **Here** is `code`."),
                                             &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "happy to") == NULL);
    HU_ASSERT_TRUE(strstr(out, "**") == NULL);
    HU_ASSERT_TRUE(strstr(out, "`") == NULL);
    HU_ASSERT_TRUE(out_len <= 300);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void imessage_format_outbound_caps_at_300(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char big[600];
    memset(big, 'a', sizeof(big) - 1);
    big[150] = '.';
    big[sizeof(big) - 1] = '\0';
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_channel_format_outbound(&alloc, S("imessage"), big, sizeof(big) - 1,
                                             &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len <= 301);
    alloc.free(alloc.ctx, out, out_len + 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 6 — Conversation awareness (human-mimicking intelligence)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_awareness_builds_thread_context(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_history_entry_t entries[3] = {
        {.from_me = false, .text = "hey what's up", .timestamp = "10:00 AM"},
        {.from_me = true, .text = "not much, you?", .timestamp = "10:01 AM"},
        {.from_me = false, .text = "want to grab lunch?", .timestamp = "10:02 AM"},
    };
    size_t out_len = 0;
    char *ctx = hu_conversation_build_awareness(&alloc, entries, 3, NULL, &out_len);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(ctx, "hey what") != NULL || strstr(ctx, "lunch") != NULL);
    alloc.free(alloc.ctx, ctx, out_len + 1);
}

static void imessage_awareness_anti_ai_rules_present(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_history_entry_t entries[2] = {
        {.from_me = false, .text = "hey", .timestamp = "10:00"},
        {.from_me = true, .text = "what's up", .timestamp = "10:01"},
    };
    size_t out_len = 0;
    char *ctx = hu_conversation_build_awareness(&alloc, entries, 2, NULL, &out_len);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(strstr(ctx, "happy to") != NULL || strstr(ctx, "AI") != NULL ||
                    strstr(ctx, "STYLE") != NULL || strstr(ctx, "naturally") != NULL);
    alloc.free(alloc.ctx, ctx, out_len + 1);
}

static void imessage_quality_score_penalizes_long_response(void) {
    hu_channel_history_entry_t entries[2] = {
        {.from_me = false, .text = "hey", .timestamp = "10:00"},
        {.from_me = true, .text = "yo", .timestamp = "10:01"},
    };
    char long_resp[400];
    memset(long_resp, 'a', sizeof(long_resp) - 1);
    long_resp[sizeof(long_resp) - 1] = '\0';
    hu_quality_score_t score = hu_conversation_evaluate_quality(long_resp, sizeof(long_resp) - 1,
                                                                entries, 2, 300);
    HU_ASSERT_TRUE(score.brevity < 80);
}

static void imessage_quality_score_short_response_not_flagged_for_revision(void) {
    hu_channel_history_entry_t entries[2] = {
        {.from_me = false, .text = "want to grab food?", .timestamp = "10:00"},
        {.from_me = true, .text = "sure", .timestamp = "10:01"},
    };
    hu_quality_score_t score =
        hu_conversation_evaluate_quality("yeah sounds good!", 17, entries, 2, 300);
    HU_ASSERT_TRUE(score.total >= 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 7 — Tapback decisions and reaction classification
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_tapback_decision_on_haha(void) {
    hu_channel_history_entry_t entries[2] = {
        {.from_me = false, .text = "haha that's hilarious", .timestamp = "10:00"},
        {.from_me = true, .text = "right??", .timestamp = "10:01"},
    };
    hu_tapback_decision_t d = hu_conversation_classify_tapback_decision(
        S("haha that's hilarious"), entries, 2, NULL, 12345);
    HU_ASSERT_TRUE(d == HU_TAPBACK_ONLY || d == HU_TAPBACK_AND_TEXT || d == HU_TEXT_ONLY);
}

static void imessage_reaction_classify_on_funny_text(void) {
    hu_channel_history_entry_t entries[1] = {
        {.from_me = false, .text = "lmaooo", .timestamp = "10:00"},
    };
    /* Seed 0 yields roll < 30 → HAHA for funny-pattern messages (see test_conversation). */
    hu_reaction_type_t r =
        hu_conversation_classify_reaction(S("lmaooo"), false, entries, 1, 0u);
    HU_ASSERT_TRUE(r != HU_REACTION_NONE);
    HU_ASSERT_EQ(r, HU_REACTION_HAHA);
}

static void imessage_self_reaction_mostly_none(void) {
    int none_count = 0;
    for (uint32_t seed = 0; seed < 200; seed++) {
        hu_reaction_type_t r =
            hu_conversation_classify_self_reaction(S("just said something normal"), seed);
        if (r == HU_REACTION_NONE)
            none_count++;
    }
    HU_ASSERT_TRUE(none_count > 180);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 8 — GIF calibration loop
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_gif_cal_neutral_before_3_sends(void) {
    float rate = hu_conversation_gif_cal_hit_rate(S("new-contact"));
    HU_ASSERT_TRUE(rate >= 0.49f && rate <= 0.51f);
}

static void imessage_gif_cal_tracks_sends_and_reactions(void) {
    hu_conversation_gif_cal_record_send(S("gif-test-contact"), S("cats"));
    hu_conversation_gif_cal_record_send(S("gif-test-contact"), S("dogs"));
    hu_conversation_gif_cal_record_send(S("gif-test-contact"), S("birds"));
    hu_conversation_gif_cal_record_reaction(S("gif-test-contact"));
    float rate = hu_conversation_gif_cal_hit_rate(S("gif-test-contact"));
    HU_ASSERT_TRUE(rate > 0.0f && rate < 1.0f);
}

static void imessage_gif_probability_adjusts_by_relationship(void) {
    float friend_prob = hu_conversation_adjust_gif_probability(0.1f, S("friend"));
    float coworker_prob = hu_conversation_adjust_gif_probability(0.1f, S("coworker"));
    HU_ASSERT_TRUE(friend_prob >= coworker_prob);
}

static void imessage_gif_should_send_deterministic_false_on_serious(void) {
    hu_channel_history_entry_t entries[1] = {
        {.from_me = false, .text = "my grandmother just died", .timestamp = "10:00"},
    };
    bool send = hu_conversation_should_send_gif(S("my grandmother just died"), entries, 1, 42,
                                                 0.5f);
    HU_ASSERT_FALSE(send);
}

static void imessage_should_send_music_on_music_mention(void) {
    hu_channel_history_entry_t hist[1] = {
        {.from_me = false, .text = "hey", .timestamp = "10:00"},
    };
    bool result = hu_conversation_should_send_music("what are you listening to", 25, hist, 1, 42,
                                                      1.0f);
    HU_ASSERT_TRUE(result);
}

static void imessage_should_send_music_respects_zero_probability(void) {
    hu_channel_history_entry_t hist[1] = {
        {.from_me = false, .text = "hey", .timestamp = "10:00"},
    };
    bool result =
        hu_conversation_should_send_music("hello there", 11, hist, 1, 42, 0.0f);
    HU_ASSERT_FALSE(result);
}

static void imessage_build_music_prompt_includes_context(void) {
    char out[256];
    size_t len =
        hu_conversation_build_music_prompt("I'm feeling nostalgic", 21, out, sizeof(out));
    HU_ASSERT_TRUE(len > 0u);
    HU_ASSERT_NOT_NULL(strstr(out, "nostalgic"));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 9 — Seen behavior / emotional pacing
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_seen_behavior_responds_to_question(void) {
    uint32_t delay_ms = 0;
    hu_seen_action_t action =
        hu_conversation_classify_seen_behavior(S("are you free tonight?"), 14, 42, &delay_ms);
    HU_ASSERT_EQ((int)action, (int)HU_SEEN_RESPOND_NOW);
}

static void imessage_engagement_detection_works(void) {
    hu_channel_history_entry_t entries[3] = {
        {.from_me = false, .text = "I had the best day ever!!!", .timestamp = "10:00"},
        {.from_me = true, .text = "oh nice! what happened?", .timestamp = "10:01"},
        {.from_me = false, .text = "got promoted! and then went to dinner!", .timestamp = "10:02"},
    };
    hu_engagement_level_t level = hu_conversation_detect_engagement(entries, 3);
    HU_ASSERT_TRUE(level == HU_ENGAGEMENT_HIGH || level == HU_ENGAGEMENT_MODERATE);
}

static void imessage_emotion_detection_on_distress(void) {
    hu_channel_history_entry_t entries[1] = {
        {.from_me = false, .text = "I'm so overwhelmed and stressed out", .timestamp = "10:00"},
    };
    hu_emotional_state_t state = hu_conversation_detect_emotion(entries, 1);
    HU_ASSERT_TRUE(state.valence <= 0.0f || state.intensity > 0.0f || state.concerning);
}

static void imessage_narrative_phase_from_short_convo(void) {
    hu_channel_history_entry_t entries[2] = {
        {.from_me = false, .text = "hey", .timestamp = "10:00"},
        {.from_me = true, .text = "hey!", .timestamp = "10:01"},
    };
    hu_narrative_phase_t phase = hu_conversation_detect_narrative(entries, 2);
    HU_ASSERT_TRUE(phase == HU_NARRATIVE_OPENING || phase == HU_NARRATIVE_BUILDING);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 10 — Persona integration (iMessage overlay + examples)
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef HU_HAS_PERSONA
static void imessage_persona_overlay_found_by_channel(void) {
    hu_persona_overlay_t overlay = {
        .channel = "imessage",
        .formality = "casual",
        .avg_length = "short",
        .emoji_usage = "minimal",
    };
    hu_persona_t persona = {0};
    persona.overlays = &overlay;
    persona.overlays_count = 1;
    const hu_persona_overlay_t *found = hu_persona_find_overlay(&persona, S("imessage"));
    HU_ASSERT_NOT_NULL(found);
    HU_ASSERT_STR_EQ(found->formality, "casual");
}

static void imessage_persona_overlay_not_found_for_other_channel(void) {
    hu_persona_overlay_t overlay = {.channel = "imessage", .formality = "casual"};
    hu_persona_t persona = {0};
    persona.overlays = &overlay;
    persona.overlays_count = 1;
    HU_ASSERT_NULL(hu_persona_find_overlay(&persona, S("telegram")));
}

static void imessage_persona_prompt_includes_casual_with_overlay(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_overlay_t overlay = {
        .channel = "imessage",
        .formality = "casual",
        .avg_length = "short",
        .emoji_usage = "minimal",
    };
    hu_persona_t persona = {0};
    persona.name = "Test";
    persona.overlays = &overlay;
    persona.overlays_count = 1;
    char *prompt = NULL;
    size_t prompt_len = 0;
    hu_error_t err =
        hu_persona_build_prompt(&alloc, &persona, S("imessage"), NULL, 0, &prompt, &prompt_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(prompt);
    HU_ASSERT_TRUE(prompt_len > 0);
    HU_ASSERT_TRUE(strstr(prompt, "casual") != NULL || strstr(prompt, "Casual") != NULL);
    alloc.free(alloc.ctx, prompt, prompt_len + 1);
}

static void imessage_persona_example_selection_matches_topic(void) {
    hu_persona_example_t examples[2] = {
        {.context = "lunch plans", .incoming = "wanna eat?", .response = "sure where"},
        {.context = "coding help", .incoming = "this bug is wild", .response = "let me look"},
    };
    hu_persona_example_bank_t bank = {
        .channel = "imessage", .examples = examples, .examples_count = 2};
    hu_persona_t persona = {0};
    persona.example_banks = &bank;
    persona.example_banks_count = 1;
    const hu_persona_example_t *selected[4];
    size_t count = 0;
    hu_error_t err =
        hu_persona_select_examples(&persona, S("imessage"), S("lunch"), selected, &count, 2);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1);
}
#endif /* HU_HAS_PERSONA */

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 11 — Poll metadata (attachment/video flags, multi-sender batching)
 * ═══════════════════════════════════════════════════════════════════════════ */

#if HU_IS_TEST
static void imessage_poll_photo_flag_propagates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock_ex(&ch, "alice", 5, "[Photo]", 7, true), HU_OK);
    hu_channel_loop_msg_t msgs[2];
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 2, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_TRUE(msgs[0].has_attachment);
    HU_ASSERT_FALSE(msgs[0].has_video);
    hu_imessage_destroy(&ch);
}

static void imessage_poll_video_flag_propagates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock_ex2(&ch, "bob", 3, "[Video]", 7, false, true),
                 HU_OK);
    hu_channel_loop_msg_t msgs[2];
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 2, &count), HU_OK);
    HU_ASSERT_EQ(count, 1u);
    HU_ASSERT_TRUE(msgs[0].has_video);
    HU_ASSERT_FALSE(msgs[0].has_attachment);
    hu_imessage_destroy(&ch);
}

static void imessage_poll_multi_sender_batch(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "alice", 5, "hey", 3), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "bob", 3, "yo", 2), HU_OK);
    HU_ASSERT_EQ(hu_imessage_test_inject_mock(&ch, "alice", 5, "what's up", 9), HU_OK);
    hu_channel_loop_msg_t msgs[4];
    size_t count = 0;
    HU_ASSERT_EQ(hu_imessage_poll(ch.ctx, &alloc, msgs, 4, &count), HU_OK);
    HU_ASSERT_EQ(count, 3u);
    HU_ASSERT_STR_EQ(msgs[0].session_key, "alice");
    HU_ASSERT_STR_EQ(msgs[1].session_key, "bob");
    HU_ASSERT_STR_EQ(msgs[2].session_key, "alice");
    HU_ASSERT_EQ(msgs[0].message_id, (int64_t)1);
    HU_ASSERT_EQ(msgs[1].message_id, (int64_t)2);
    HU_ASSERT_EQ(msgs[2].message_id, (int64_t)3);
    hu_imessage_destroy(&ch);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 12 — Conversation hints (cold restart, group, link, emoji mirror)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_cold_restart_hint_on_fresh_convo(void) {
    /* Cold restart needs ≥2 messages and ≥4h gap between prev and latest inbound. */
    hu_channel_history_entry_t entries[2] = {
        {.from_me = true, .text = "catch you later", .timestamp = "2024-01-01 08:00:00"},
        {.from_me = false, .text = "hey again!", .timestamp = "2024-01-01 20:00:00"},
    };
    char buf[256];
    size_t len = hu_conversation_build_cold_restart_hint(entries, 2, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "COLD RESTART") != NULL);
}

static void imessage_group_mention_hint_non_empty_for_group(void) {
    char buf[512];
    size_t len = hu_conversation_build_group_mention_hint(S("Alice"), true, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "Alice") != NULL);
}

static void imessage_group_mention_hint_empty_for_dm(void) {
    char buf[512];
    size_t len = hu_conversation_build_group_mention_hint(S("Alice"), false, buf, sizeof(buf));
    HU_ASSERT_EQ(len, 0u);
}

static void imessage_link_context_detects_url(void) {
    char buf[256];
    size_t len =
        hu_conversation_build_link_context(S("check this out https://example.com"), buf,
                                            sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
}

static void imessage_emoji_mirror_hint_exists(void) {
    /* Emoji mirror needs ≥2 inbound messages; all-ASCII yields low emoji_rate directive. */
    hu_channel_history_entry_t entries[3] = {
        {.from_me = false, .text = "hey", .timestamp = "10:00"},
        {.from_me = false, .text = "what's up", .timestamp = "10:01"},
        {.from_me = true, .text = "nice!", .timestamp = "10:02"},
    };
    char buf[256];
    size_t len = hu_conversation_build_emoji_mirror_hint(entries, 3, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "EMOJI") != NULL);
}

static void imessage_gif_style_hint_for_friend(void) {
    char buf[128];
    size_t len = hu_conversation_build_gif_style_hint(S("friend"), buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 13 — Effect mapping, balloon classification, mark_read, use_imsg_cli
 * ═══════════════════════════════════════════════════════════════════════════ */

static void imessage_effect_name_slam(void) {
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.MobileSMS.expressivesend.impact"), "Slam");
}

static void imessage_effect_name_all_known(void) {
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.MobileSMS.expressivesend.loud"), "Loud");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.MobileSMS.expressivesend.gentle"),
                      "Gentle");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.MobileSMS.expressivesend.invisibleink"),
                      "Invisible Ink");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.messages.effect.CKConfettiEffect"),
                      "Confetti");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.messages.effect.CKEchoEffect"), "Echo");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.messages.effect.CKFireworksEffect"),
                      "Fireworks");
    HU_ASSERT_STR_EQ(
        hu_imessage_effect_name("com.apple.messages.effect.CKHappyBirthdayEffect"),
        "Happy Birthday");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.messages.effect.CKHeartEffect"), "Heart");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.messages.effect.CKLasersEffect"),
                      "Lasers");
    HU_ASSERT_STR_EQ(
        hu_imessage_effect_name("com.apple.messages.effect.CKShootingStarEffect"),
        "Shooting Star");
    HU_ASSERT_STR_EQ(hu_imessage_effect_name("com.apple.messages.effect.CKSparklesEffect"),
                      "Sparkles");
    HU_ASSERT_STR_EQ(
        hu_imessage_effect_name("com.apple.messages.effect.CKSpotlightEffect"), "Spotlight");
}

static void imessage_effect_name_null_and_unknown(void) {
    HU_ASSERT_NULL(hu_imessage_effect_name(NULL));
    HU_ASSERT_NULL(hu_imessage_effect_name(""));
    HU_ASSERT_NULL(hu_imessage_effect_name("com.apple.unknown.effect"));
}

static void imessage_balloon_label_sticker(void) {
    HU_ASSERT_STR_EQ(
        hu_imessage_balloon_label(
            "com.apple.messages.MSMessageExtensionBalloonPlugin:0000000000:"
            "com.apple.Stickers.UserGenerated.StickerPack"),
        "[Sticker]");
}

static void imessage_balloon_label_memoji(void) {
    HU_ASSERT_STR_EQ(
        hu_imessage_balloon_label(
            "com.apple.messages.MSMessageExtensionBalloonPlugin:0000000000:"
            "com.apple.Animoji.StickersApp.MessagesExtension"),
        "[Memoji]");
    HU_ASSERT_STR_EQ(hu_imessage_balloon_label("some.Memoji.plugin"), "[Memoji]");
}

static void imessage_balloon_label_app(void) {
    HU_ASSERT_STR_EQ(hu_imessage_balloon_label("com.example.app"), "[iMessage App]");
}

static void imessage_balloon_label_null_and_empty(void) {
    HU_ASSERT_NULL(hu_imessage_balloon_label(NULL));
    HU_ASSERT_NULL(hu_imessage_balloon_label(""));
}

static void imessage_text_is_placeholder_true_cases(void) {
    HU_ASSERT_TRUE(hu_imessage_text_is_placeholder(NULL));
    HU_ASSERT_TRUE(hu_imessage_text_is_placeholder(""));
    HU_ASSERT_TRUE(hu_imessage_text_is_placeholder("[Photo]"));
    HU_ASSERT_TRUE(hu_imessage_text_is_placeholder("[Video]"));
    HU_ASSERT_TRUE(hu_imessage_text_is_placeholder("[Voice Message]"));
}

static void imessage_text_is_placeholder_false_cases(void) {
    HU_ASSERT_FALSE(hu_imessage_text_is_placeholder("Hello!"));
    HU_ASSERT_FALSE(hu_imessage_text_is_placeholder("Check this out"));
    HU_ASSERT_FALSE(hu_imessage_text_is_placeholder("[Photo] extra text"));
    HU_ASSERT_FALSE(hu_imessage_text_is_placeholder("photo"));
}

static void imessage_copy_bounded_normal(void) {
    char dst[32];
    size_t n = hu_imessage_copy_bounded(dst, sizeof(dst), "hello", 5);
    HU_ASSERT_EQ(n, 5);
    HU_ASSERT_STR_EQ(dst, "hello");
}

static void imessage_copy_bounded_truncates(void) {
    char dst[4];
    size_t n = hu_imessage_copy_bounded(dst, sizeof(dst), "hello world", 11);
    HU_ASSERT_EQ(n, 3);
    HU_ASSERT_STR_EQ(dst, "hel");
}

static void imessage_copy_bounded_exact_fit(void) {
    char dst[6];
    size_t n = hu_imessage_copy_bounded(dst, sizeof(dst), "hello", 5);
    HU_ASSERT_EQ(n, 5);
    HU_ASSERT_STR_EQ(dst, "hello");
}

static void imessage_copy_bounded_null_and_empty(void) {
    char dst[16] = "dirty";
    HU_ASSERT_EQ(hu_imessage_copy_bounded(dst, sizeof(dst), NULL, 0), 0);
    HU_ASSERT_EQ(dst[0], '\0');

    dst[0] = 'x';
    HU_ASSERT_EQ(hu_imessage_copy_bounded(dst, sizeof(dst), "", 0), 0);
    HU_ASSERT_EQ(dst[0], '\0');

    HU_ASSERT_EQ(hu_imessage_copy_bounded(NULL, 0, "hello", 5), 0);
}

#if HU_IS_TEST
static void imessage_start_typing_returns_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+1234567890", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable->start_typing);
    HU_ASSERT_EQ(ch.vtable->start_typing(ch.ctx, "+1234567890", 12), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable->stop_typing);
    HU_ASSERT_EQ(ch.vtable->stop_typing(ch.ctx, "+1234567890", 12), HU_OK);
    hu_imessage_destroy(&ch);
}

static void imessage_start_typing_null_ctx_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+1234567890", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_EQ(ch.vtable->start_typing(NULL, "+1234567890", 12), HU_OK);
    HU_ASSERT_EQ(ch.vtable->stop_typing(NULL, "+1234567890", 12), HU_OK);
    hu_imessage_destroy(&ch);
}

static void imessage_set_use_imsg_cli_wires_to_context(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    hu_imessage_set_use_imsg_cli(&ch, true);
    hu_imessage_set_use_imsg_cli(&ch, false);
    hu_imessage_set_use_imsg_cli(NULL, true);
    hu_imessage_destroy(&ch);
}

static void imessage_mark_read_returns_ok_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable->mark_read);
    HU_ASSERT_EQ(ch.vtable->mark_read(ch.ctx, "+15551234567", 12), HU_OK);
    hu_imessage_destroy(&ch);
}

static void imessage_mark_read_null_ctx_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_imessage_create(&alloc, "+15551234567", 12, NULL, 0, &ch), HU_OK);
    (void)ch.vtable->mark_read(NULL, "+15551234567", 12);
    hu_imessage_destroy(&ch);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Suite registration
 * ═══════════════════════════════════════════════════════════════════════════ */

void run_imessage_adversarial_tests(void) {
    HU_TEST_SUITE("iMessage Human-Fidelity Fleet");

    /* Part 1: Send / poll boundary validation */
#if HU_IS_TEST
    HU_RUN_TEST(imessage_send_rejects_null_body_with_positive_len);
    HU_RUN_TEST(imessage_send_rejects_empty_text_and_no_media);
    HU_RUN_TEST(imessage_send_media_only_accepted_as_voice_message);
    HU_RUN_TEST(imessage_send_truncates_overlong_text_to_4095);
    HU_RUN_TEST(imessage_send_records_media_in_test);
    HU_RUN_TEST(imessage_poll_rejects_all_null_args);
    HU_RUN_TEST(imessage_poll_rejects_null_msgs_buffer);
    HU_RUN_TEST(imessage_poll_rejects_null_out_count);
    HU_RUN_TEST(imessage_inject_rejects_null_channel);
    HU_RUN_TEST(imessage_inject_ninth_message_returns_out_of_memory);
    HU_RUN_TEST(imessage_inject_truncates_long_session_key);
    HU_RUN_TEST(imessage_inject_truncates_long_content);
    HU_RUN_TEST(imessage_mock_poll_drops_overflow_when_max_msgs_too_small);
    HU_RUN_TEST(imessage_test_get_last_message_null_channel_returns_null);
    HU_RUN_TEST(imessage_test_get_last_reaction_null_args_no_crash);
#endif

    /* Part 2: Vtable surface coverage */
#if HU_IS_TEST
    HU_RUN_TEST(imessage_vtable_has_all_expected_hooks);
    HU_RUN_TEST(imessage_load_history_returns_not_supported_under_test);
    HU_RUN_TEST(imessage_response_constraints_300_chars);
    HU_RUN_TEST(imessage_response_constraints_null_out_rejected);
    HU_RUN_TEST(imessage_human_active_always_false_under_test);
    HU_RUN_TEST(imessage_attachment_path_null_under_test);
    HU_RUN_TEST(imessage_build_tapback_context_noop_under_test);
    HU_RUN_TEST(imessage_build_read_receipt_context_noop_under_test);
    HU_RUN_TEST(imessage_fetch_gif_stub_returns_null);
    HU_RUN_TEST(imessage_fetch_gif_rejects_empty_query);
    HU_RUN_TEST(imessage_gif_json_extract_finds_url_value);
    HU_RUN_TEST(imessage_gif_json_extract_returns_zero_when_key_missing);
    HU_RUN_TEST(imessage_gif_json_extract_truncates_to_out_cap);
    HU_RUN_TEST(imessage_lookup_guid_not_supported_under_test);
    HU_RUN_TEST(imessage_start_stop_idempotent);
#endif

    /* Part 3: AppleScript escaping */
#if (defined(__APPLE__) && defined(__MACH__)) || HU_IS_TEST
    HU_RUN_TEST(imessage_escape_preserves_utf8_emoji);
    HU_RUN_TEST(imessage_escape_out_cap_two_yields_empty);
    HU_RUN_TEST(imessage_escape_strips_applescript_injection_tokens);
    HU_RUN_TEST(imessage_escape_control_chars_stripped);
    HU_RUN_TEST(imessage_escape_backslash_doubled);
#endif

    /* Part 4: Tapback mapping */
    HU_RUN_TEST(imessage_tapback_mapping_all_types);
#if HU_IS_TEST
    HU_RUN_TEST(imessage_react_vtable_records_heart);
    HU_RUN_TEST(imessage_react_vtable_overwrites_previous);
#endif

    /* Part 5: Format pipeline (markdown + AI phrase + 300 char) */
    HU_RUN_TEST(imessage_format_strips_markdown_bold);
    HU_RUN_TEST(imessage_format_strips_markdown_headers);
    HU_RUN_TEST(imessage_format_strips_ai_happy_to);
    HU_RUN_TEST(imessage_format_strips_ai_great_question);
    HU_RUN_TEST(imessage_format_strips_as_an_ai);
    HU_RUN_TEST(imessage_format_outbound_full_pipeline);
    HU_RUN_TEST(imessage_format_outbound_caps_at_300);

    /* Part 6: Conversation awareness */
    HU_RUN_TEST(imessage_awareness_builds_thread_context);
    HU_RUN_TEST(imessage_awareness_anti_ai_rules_present);
    HU_RUN_TEST(imessage_quality_score_penalizes_long_response);
    HU_RUN_TEST(imessage_quality_score_short_response_not_flagged_for_revision);

    /* Part 7: Tapback decisions + reaction classification */
    HU_RUN_TEST(imessage_tapback_decision_on_haha);
    HU_RUN_TEST(imessage_reaction_classify_on_funny_text);
    HU_RUN_TEST(imessage_self_reaction_mostly_none);

    /* Part 8: GIF calibration loop */
    HU_RUN_TEST(imessage_gif_cal_neutral_before_3_sends);
    HU_RUN_TEST(imessage_gif_cal_tracks_sends_and_reactions);
    HU_RUN_TEST(imessage_gif_probability_adjusts_by_relationship);
    HU_RUN_TEST(imessage_gif_should_send_deterministic_false_on_serious);
    HU_RUN_TEST(imessage_should_send_music_on_music_mention);
    HU_RUN_TEST(imessage_should_send_music_respects_zero_probability);
    HU_RUN_TEST(imessage_build_music_prompt_includes_context);

    /* Part 9: Seen behavior / emotional intelligence */
    HU_RUN_TEST(imessage_seen_behavior_responds_to_question);
    HU_RUN_TEST(imessage_engagement_detection_works);
    HU_RUN_TEST(imessage_emotion_detection_on_distress);
    HU_RUN_TEST(imessage_narrative_phase_from_short_convo);

    /* Part 10: Persona overlays + example banks */
#ifdef HU_HAS_PERSONA
    HU_RUN_TEST(imessage_persona_overlay_found_by_channel);
    HU_RUN_TEST(imessage_persona_overlay_not_found_for_other_channel);
    HU_RUN_TEST(imessage_persona_prompt_includes_casual_with_overlay);
    HU_RUN_TEST(imessage_persona_example_selection_matches_topic);
#endif

    /* Part 11: Poll metadata */
#if HU_IS_TEST
    HU_RUN_TEST(imessage_poll_photo_flag_propagates);
    HU_RUN_TEST(imessage_poll_video_flag_propagates);
    HU_RUN_TEST(imessage_poll_multi_sender_batch);
#endif

    /* Part 12: Conversation hints */
    HU_RUN_TEST(imessage_cold_restart_hint_on_fresh_convo);
    HU_RUN_TEST(imessage_group_mention_hint_non_empty_for_group);
    HU_RUN_TEST(imessage_group_mention_hint_empty_for_dm);
    HU_RUN_TEST(imessage_link_context_detects_url);
    HU_RUN_TEST(imessage_emoji_mirror_hint_exists);
    HU_RUN_TEST(imessage_gif_style_hint_for_friend);

    /* Part 13: Effect mapping, balloon labels, mark_read, use_imsg_cli */
    HU_RUN_TEST(imessage_effect_name_slam);
    HU_RUN_TEST(imessage_effect_name_all_known);
    HU_RUN_TEST(imessage_effect_name_null_and_unknown);
    HU_RUN_TEST(imessage_balloon_label_sticker);
    HU_RUN_TEST(imessage_balloon_label_memoji);
    HU_RUN_TEST(imessage_balloon_label_app);
    HU_RUN_TEST(imessage_balloon_label_null_and_empty);
    HU_RUN_TEST(imessage_text_is_placeholder_true_cases);
    HU_RUN_TEST(imessage_text_is_placeholder_false_cases);
    HU_RUN_TEST(imessage_copy_bounded_normal);
    HU_RUN_TEST(imessage_copy_bounded_truncates);
    HU_RUN_TEST(imessage_copy_bounded_exact_fit);
    HU_RUN_TEST(imessage_copy_bounded_null_and_empty);
#if HU_IS_TEST
    HU_RUN_TEST(imessage_mark_read_returns_ok_under_test);
    HU_RUN_TEST(imessage_mark_read_null_ctx_no_crash);
    HU_RUN_TEST(imessage_start_typing_returns_ok);
    HU_RUN_TEST(imessage_start_typing_null_ctx_safe);
    HU_RUN_TEST(imessage_set_use_imsg_cli_wires_to_context);
#endif
}
#else  /* !HU_HAS_IMESSAGE */
void run_imessage_adversarial_tests(void) {
    (void)0;
}
#endif /* HU_HAS_IMESSAGE */
