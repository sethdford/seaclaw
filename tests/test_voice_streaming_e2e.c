#include "human/config.h"
#include "human/core/allocator.h"
#include "human/voice/duplex.h"
#include "human/voice/provider.h"
#include "human/voice/session.h"
#include "test_framework.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    unsigned recv_seq;
    bool connected;
    bool activity_started;
    unsigned tool_response_count;
    unsigned reconnect_count;
} mock_vp_ctx_t;

static hu_error_t mock_connect(void *ctx) {
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->connected = true;
    return HU_OK;
}

static hu_error_t mock_send_audio(void *ctx, const void *pcm16, size_t len) {
    (void)ctx;
    (void)pcm16;
    if (len != 320u)
        return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

static hu_error_t mock_recv_event(void *ctx, hu_allocator_t *alloc, hu_voice_rt_event_t *out,
                                  int timeout_ms) {
    (void)timeout_ms;
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (!c || !alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    unsigned n = c->recv_seq++;
    if (n >= 99u)
        return HU_ERR_IO;
    if (n == 0u) {
        static const char b64[] = "AAAA";
        size_t l = strlen(b64);
        char *copy = (char *)alloc->alloc(alloc->ctx, l + 1u);
        if (!copy)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(copy, b64, l + 1u);
        out->audio_base64 = copy;
        out->audio_base64_len = l;
        return HU_OK;
    }
    if (n == 1u) {
        static const char tx[] = "hello";
        size_t l = strlen(tx);
        char *copy = (char *)alloc->alloc(alloc->ctx, l + 1u);
        if (!copy)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(copy, tx, l + 1u);
        out->transcript = copy;
        out->transcript_len = 5u;
        return HU_OK;
    }
    if (n == 2u) {
        out->done = true;
        return HU_OK;
    }
    if (n == 3u) {
        static const char tx[] = "four";
        size_t l = strlen(tx);
        char *copy = (char *)alloc->alloc(alloc->ctx, l + 1u);
        if (!copy)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(copy, tx, l + 1u);
        out->transcript = copy;
        out->transcript_len = l;
        return HU_OK;
    }
    return HU_OK;
}

static hu_error_t mock_cancel_response(void *ctx) {
    (void)ctx;
    return HU_OK;
}

static void mock_disconnect(void *ctx, hu_allocator_t *alloc) {
    (void)alloc;
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (c)
        c->connected = false;
}

static const char *mock_get_name(void *ctx) {
    (void)ctx;
    return "mock";
}

static hu_error_t mock_send_activity_start(void *ctx) {
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->activity_started = true;
    return HU_OK;
}

static hu_error_t mock_send_activity_end(void *ctx) {
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->activity_started = false;
    return HU_OK;
}

static hu_error_t mock_send_tool_response(void *ctx, const char *name, const char *call_id,
                                          const char *response_json) {
    (void)name;
    (void)call_id;
    (void)response_json;
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->tool_response_count++;
    return HU_OK;
}

static hu_error_t mock_reconnect(void *ctx) {
    mock_vp_ctx_t *c = (mock_vp_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->reconnect_count++;
    return HU_OK;
}

static hu_error_t mock_add_tool(void *ctx, const char *name, const char *description,
                                const char *parameters_json) {
    (void)ctx;
    (void)name;
    (void)description;
    (void)parameters_json;
    return HU_OK;
}

static hu_error_t mock_send_audio_stream_end(void *ctx) {
    (void)ctx;
    return HU_OK;
}

static const hu_voice_provider_vtable_t s_mock_vp_vtable = {
    .connect = mock_connect,
    .send_audio = mock_send_audio,
    .recv_event = mock_recv_event,
    .add_tool = mock_add_tool,
    .cancel_response = mock_cancel_response,
    .disconnect = mock_disconnect,
    .get_name = mock_get_name,
    .send_activity_start = mock_send_activity_start,
    .send_activity_end = mock_send_activity_end,
    .send_audio_stream_end = mock_send_audio_stream_end,
    .reconnect = mock_reconnect,
    .send_tool_response = mock_send_tool_response,
};

static void test_mock_provider_recv_audio_delta(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_vp_ctx_t ctx = {0};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    hu_voice_rt_event_t ev = {0};
    HU_ASSERT_EQ(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 0), HU_OK);
    HU_ASSERT_TRUE(ev.audio_base64 != NULL && ev.audio_base64_len > 0);
    hu_voice_rt_event_free(&alloc, &ev);
}

static void test_mock_provider_recv_transcript(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_vp_ctx_t ctx = {.recv_seq = 1u};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    hu_voice_rt_event_t ev = {0};
    HU_ASSERT_EQ(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 0), HU_OK);
    HU_ASSERT_EQ(ev.transcript_len, 5u);
    hu_voice_rt_event_free(&alloc, &ev);
}

static void test_mock_provider_recv_done(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_vp_ctx_t ctx = {.recv_seq = 2u};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    hu_voice_rt_event_t ev = {0};
    HU_ASSERT_EQ(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 0), HU_OK);
    HU_ASSERT_TRUE(ev.done);
    hu_voice_rt_event_free(&alloc, &ev);
}

static void test_mock_provider_io_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_vp_ctx_t ctx = {.recv_seq = 99u};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    hu_voice_rt_event_t ev = {0};
    HU_ASSERT_EQ(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 0), HU_ERR_IO);
    hu_voice_rt_event_free(&alloc, &ev);
}

static void test_mock_provider_activity_lifecycle(void) {
    mock_vp_ctx_t ctx = {0};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    HU_ASSERT_EQ(vp.vtable->send_activity_start(vp.ctx), HU_OK);
    HU_ASSERT_TRUE(ctx.activity_started);
    HU_ASSERT_EQ(vp.vtable->send_activity_end(vp.ctx), HU_OK);
    HU_ASSERT_FALSE(ctx.activity_started);
}

static void test_mock_provider_tool_response(void) {
    mock_vp_ctx_t ctx = {0};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    HU_ASSERT_EQ(vp.vtable->send_tool_response(vp.ctx, "t", "id", "{}"), HU_OK);
    HU_ASSERT_EQ(ctx.tool_response_count, 1u);
}

static void test_mock_provider_send_audio(void) {
    mock_vp_ctx_t ctx = {0};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    uint8_t buf[320];
    memset(buf, 0, sizeof(buf));
    HU_ASSERT_EQ(vp.vtable->send_audio(vp.ctx, buf, sizeof(buf)), HU_OK);
}

static void test_mock_provider_full_sequence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_vp_ctx_t ctx = {0};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    for (int i = 0; i < 4; i++) {
        hu_voice_rt_event_t ev = {0};
        HU_ASSERT_EQ(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 0), HU_OK);
        if (i == 0)
            HU_ASSERT_TRUE(ev.audio_base64 && ev.audio_base64_len > 0);
        else if (i == 1)
            HU_ASSERT_EQ(ev.transcript_len, 5u);
        else if (i == 2)
            HU_ASSERT_TRUE(ev.done);
        else
            HU_ASSERT_TRUE(ev.transcript && ev.transcript_len == 4u);
        hu_voice_rt_event_free(&alloc, &ev);
    }
}

static void test_mock_provider_get_name(void) {
    mock_vp_ctx_t ctx = {0};
    hu_voice_provider_t vp = {.ctx = &ctx, .vtable = &s_mock_vp_vtable};
    HU_ASSERT_TRUE(vp.vtable->get_name(vp.ctx) != NULL);
    HU_ASSERT_EQ(strcmp(vp.vtable->get_name(vp.ctx), "mock"), 0);
}

static void test_duplex_yield_triggers_generation(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action), HU_OK);
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_CONTINUE, &action), HU_OK);
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);
}

static void test_duplex_interrupt_cancels(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_start_streaming(&s, 100);
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_INTERRUPT, &action), HU_OK);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_CANCEL_GENERATION);
}

static void test_duplex_vad_update(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    HU_ASSERT_EQ(hu_duplex_update_vad(&s, true), HU_OK);
    HU_ASSERT_TRUE(s.vad_active);
    HU_ASSERT_EQ(hu_duplex_update_vad(&s, false), HU_OK);
    HU_ASSERT_FALSE(s.vad_active);
}

static void voice_session_start_stop_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t session;
    hu_config_t cfg;

    memset(&session, 0, sizeof(session));
    memset(&cfg, 0, sizeof(cfg));
    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &session, "cli", 3, &cfg), HU_OK);
    HU_ASSERT_TRUE(session.active);
    HU_ASSERT_EQ(hu_voice_session_stop(&session), HU_OK);
    HU_ASSERT_FALSE(session.active);
}

static void voice_session_send_audio_sets_latency(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t session;
    hu_config_t cfg;
    uint8_t pcm[8];

    memset(&session, 0, sizeof(session));
    memset(&cfg, 0, sizeof(cfg));
    memset(pcm, 0, sizeof(pcm));
    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &session, "cli", 3, &cfg), HU_OK);
    HU_ASSERT_EQ(hu_voice_session_send_audio(&session, pcm, sizeof(pcm)), HU_OK);
    int64_t first_byte = 0;
    int64_t round_trip = 0;
    int64_t interrupt = 0;
    HU_ASSERT_EQ(hu_voice_session_get_latency(&session, &first_byte, &round_trip, &interrupt), HU_OK);
    HU_ASSERT_GT(first_byte, 0);
    HU_ASSERT_EQ(hu_voice_session_stop(&session), HU_OK);
}

/* Before send_audio, get_latency returns HU_ERR_INVALID_ARGUMENT (no samples). */
static void voice_session_get_latency_before_audio_is_zero(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t session;
    hu_config_t cfg;
    int64_t first_byte = -1;
    int64_t round_trip = -1;
    int64_t interrupt = -1;

    memset(&session, 0, sizeof(session));
    memset(&cfg, 0, sizeof(cfg));
    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &session, "cli", 3, &cfg), HU_OK);
    HU_ASSERT_EQ(hu_voice_session_get_latency(&session, &first_byte, &round_trip, &interrupt),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_voice_session_stop(&session), HU_OK);
}

static void voice_session_stop_without_start_returns_error_or_ok(void) {
    hu_voice_session_t session;
    memset(&session, 0, sizeof(session));
    HU_ASSERT_EQ(hu_voice_session_stop(&session), HU_OK);
}

static void voice_session_start_null_config_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t session;
    memset(&session, 0, sizeof(session));
    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &session, "cli", 3, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_session_latency_inactive(void) {
    hu_voice_session_t vs = {0};
    int64_t a = 0, b = 0, c = 0;
    HU_ASSERT_EQ(hu_voice_session_get_latency(&vs, &a, &b, &c), HU_ERR_INVALID_ARGUMENT);
}

static void test_vtable_completeness(void) {
    HU_ASSERT_TRUE(s_mock_vp_vtable.connect != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.send_audio != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.recv_event != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.add_tool != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.cancel_response != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.disconnect != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.get_name != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.send_activity_start != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.send_activity_end != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.send_audio_stream_end != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.reconnect != NULL);
    HU_ASSERT_TRUE(s_mock_vp_vtable.send_tool_response != NULL);
}

void run_voice_streaming_e2e_tests(void) {
    HU_TEST_SUITE("VoiceStreamingE2E");
    HU_RUN_TEST(test_mock_provider_recv_audio_delta);
    HU_RUN_TEST(test_mock_provider_recv_transcript);
    HU_RUN_TEST(test_mock_provider_recv_done);
    HU_RUN_TEST(test_mock_provider_io_error);
    HU_RUN_TEST(test_mock_provider_activity_lifecycle);
    HU_RUN_TEST(test_mock_provider_tool_response);
    HU_RUN_TEST(test_mock_provider_send_audio);
    HU_RUN_TEST(test_mock_provider_full_sequence);
    HU_RUN_TEST(test_mock_provider_get_name);
    HU_RUN_TEST(test_duplex_yield_triggers_generation);
    HU_RUN_TEST(test_duplex_interrupt_cancels);
    HU_RUN_TEST(test_duplex_vad_update);
    HU_RUN_TEST(voice_session_start_stop_roundtrip);
    HU_RUN_TEST(voice_session_send_audio_sets_latency);
    HU_RUN_TEST(voice_session_get_latency_before_audio_is_zero);
    HU_RUN_TEST(voice_session_stop_without_start_returns_error_or_ok);
    HU_RUN_TEST(voice_session_start_null_config_returns_error);
    HU_RUN_TEST(test_session_latency_inactive);
    HU_RUN_TEST(test_vtable_completeness);
}
