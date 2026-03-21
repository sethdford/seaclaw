#include "human/core/allocator.h"
#include "human/voice/realtime.h"
#include "test_framework.h"
#include <string.h>

/* OpenAI Realtime WebSocket session API (src/voice/realtime.c) — mock paths under HU_IS_TEST. */

static void voice_rt_openai_session_create_connect_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    cfg.model = "test-model";
    cfg.sample_rate = 16000;
    hu_voice_rt_session_t *session = NULL;
    hu_error_t err = hu_voice_rt_session_create(&alloc, &cfg, &session);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(session);
    err = hu_voice_rt_connect(session);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(session->connected);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_recv_event_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    cfg.model = "test-model";
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, &cfg, &session);
    HU_ASSERT_NOT_NULL(session);
    hu_voice_rt_connect(session);
    /* Test mock alternates events: even seq → transcription, odd → response.audio.done */
    hu_voice_rt_event_t event = {0};
    hu_error_t err = hu_voice_rt_recv_event(session, &alloc, &event, 1000);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(event.type, "conversation.item.input_audio_transcription.completed");
    HU_ASSERT_FALSE(event.done);
    HU_ASSERT_NOT_NULL(event.transcript);
    HU_ASSERT_STR_EQ(event.transcript, "Hello from realtime");
    hu_voice_rt_event_free(&alloc, &event);
    memset(&event, 0, sizeof(event));
    err = hu_voice_rt_recv_event(session, &alloc, &event, 1000);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(event.done);
    HU_ASSERT_STR_EQ(event.type, "response.audio.done");
    hu_voice_rt_event_free(&alloc, &event);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_add_tool_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    cfg.model = "test-model";
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, &cfg, &session);
    HU_ASSERT_NOT_NULL(session);
    hu_voice_rt_connect(session);
    hu_error_t err = hu_voice_rt_add_tool(session, "web_search", "Search the web", "{\"type\":\"object\"}");
    HU_ASSERT_EQ(err, HU_OK);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_recv_event_not_connected_returns_io(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, NULL, &session);
    HU_ASSERT_NOT_NULL(session);
    HU_ASSERT_FALSE(session->connected);
    hu_voice_rt_event_t event = {0};
    hu_error_t err = hu_voice_rt_recv_event(session, &alloc, &event, 0);
    HU_ASSERT_EQ(err, HU_ERR_IO);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_add_tool_null_name_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, NULL, &session);
    HU_ASSERT_NOT_NULL(session);
    hu_voice_rt_connect(session);
    hu_error_t err = hu_voice_rt_add_tool(session, NULL, "d", "{}");
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_recv_event_null_session_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_event_t event = {0};
    hu_error_t err = hu_voice_rt_recv_event(NULL, &alloc, &event, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_rt_openai_send_audio_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, NULL, &session);
    hu_voice_rt_connect(session);
    unsigned char pcm[] = {0, 0, 1, 2};
    hu_error_t err = hu_voice_rt_send_audio(session, pcm, sizeof(pcm));
    HU_ASSERT_EQ(err, HU_OK);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_session_destroy_null_safe(void) {
    /* Crash safety test: verifies NULL session does not cause segfault.
     * hu_voice_rt_session_destroy is void — no return code to assert. */
    hu_voice_rt_session_destroy(NULL);
}

static void voice_rt_openai_event_free_null_event_safe(void) {
    /* Crash safety test: verifies NULL event does not cause segfault.
     * hu_voice_rt_event_free is void — no return code to assert. */
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_event_free(&alloc, NULL);
}

static void voice_rt_openai_session_create_null_out_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    hu_error_t err = hu_voice_rt_session_create(&alloc, &cfg, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_rt_openai_send_audio_null_data_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, NULL, &session);
    hu_voice_rt_connect(session);
    hu_error_t err = hu_voice_rt_send_audio(session, NULL, 4);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_connect_null_session_fails(void) {
    hu_error_t err = hu_voice_rt_connect(NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_rt_openai_recv_null_out_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, NULL, &session);
    hu_voice_rt_connect(session);
    hu_error_t err = hu_voice_rt_recv_event(session, &alloc, NULL, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_recv_null_alloc_fails(void) {
    hu_voice_rt_session_t *session = NULL;
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_create(&alloc, NULL, &session);
    hu_voice_rt_connect(session);
    hu_voice_rt_event_t event = {0};
    hu_error_t err = hu_voice_rt_recv_event(session, NULL, &event, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_event_free_clears_owned_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_event_t ev = {0};
    static const char blob[] = "AQIDBA==";
    ev.audio_base64 = (char *)alloc.alloc(alloc.ctx, sizeof(blob));
    HU_ASSERT_NOT_NULL(ev.audio_base64);
    memcpy(ev.audio_base64, blob, sizeof(blob));
    ev.audio_base64_len = sizeof(blob) - 1;
    hu_voice_rt_event_free(&alloc, &ev);
    HU_ASSERT_NULL(ev.audio_base64);
    HU_ASSERT_EQ(ev.audio_base64_len, 0u);
}

static void voice_rt_openai_session_create_null_alloc_fails(void) {
    hu_voice_rt_config_t cfg = {0};
    hu_voice_rt_session_t *session = NULL;
    hu_error_t err = hu_voice_rt_session_create(NULL, &cfg, &session);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_rt_openai_response_cancel_mock_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_session_t *session = NULL;
    hu_voice_rt_session_create(&alloc, NULL, &session);
    hu_voice_rt_connect(session);
    HU_ASSERT_EQ(hu_voice_rt_response_cancel(session), HU_OK);
    hu_voice_rt_session_destroy(session);
}

static void voice_rt_openai_response_cancel_null_session_fails(void) {
    HU_ASSERT_EQ(hu_voice_rt_response_cancel(NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_voice_rt_openai_tests(void) {
    HU_TEST_SUITE("voice_rt_openai");
    HU_RUN_TEST(voice_rt_openai_session_create_connect_destroy);
    HU_RUN_TEST(voice_rt_openai_recv_event_mock);
    HU_RUN_TEST(voice_rt_openai_add_tool_mock);
    HU_RUN_TEST(voice_rt_openai_recv_event_not_connected_returns_io);
    HU_RUN_TEST(voice_rt_openai_add_tool_null_name_fails);
    HU_RUN_TEST(voice_rt_openai_recv_event_null_session_fails);
    HU_RUN_TEST(voice_rt_openai_send_audio_mock_ok);
    HU_RUN_TEST(voice_rt_openai_session_destroy_null_safe);
    HU_RUN_TEST(voice_rt_openai_event_free_null_event_safe);
    HU_RUN_TEST(voice_rt_openai_session_create_null_out_fails);
    HU_RUN_TEST(voice_rt_openai_send_audio_null_data_fails);
    HU_RUN_TEST(voice_rt_openai_connect_null_session_fails);
    HU_RUN_TEST(voice_rt_openai_recv_null_out_fails);
    HU_RUN_TEST(voice_rt_openai_recv_null_alloc_fails);
    HU_RUN_TEST(voice_rt_openai_event_free_clears_owned_fields);
    HU_RUN_TEST(voice_rt_openai_session_create_null_alloc_fails);
    HU_RUN_TEST(voice_rt_openai_response_cancel_mock_ok);
    HU_RUN_TEST(voice_rt_openai_response_cancel_null_session_fails);
}
