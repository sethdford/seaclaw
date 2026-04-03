#include "human/core/allocator.h"
#include "human/voice/gemini_live.h"
#include "human/voice/provider.h"
#include "human/voice/realtime.h"
#include "test_framework.h"
#include <string.h>

/* Voice Provider vtable abstraction tests (src/voice/realtime.c, src/voice/gemini_live.c).
 * All paths run under HU_IS_TEST — no real network. */

/* ── OpenAI Realtime provider ─────────────────────────────────────── */

static void voice_provider_openai_create_connect_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    cfg.model = "test-model";
    cfg.sample_rate = 16000;
    hu_voice_provider_t p = {0};
    hu_error_t err = hu_voice_provider_openai_create(&alloc, &cfg, &p);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(p.vtable);
    HU_ASSERT_NOT_NULL(p.ctx);
    err = p.vtable->connect(p.ctx);
    HU_ASSERT_EQ(err, HU_OK);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_openai_send_audio_dispatches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_openai_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    unsigned char pcm[] = {0, 1, 2, 3};
    hu_error_t err = p.vtable->send_audio(p.ctx, pcm, sizeof(pcm));
    HU_ASSERT_EQ(err, HU_OK);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_openai_recv_event_dispatches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_openai_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    hu_voice_rt_event_t ev = {0};
    hu_error_t err = p.vtable->recv_event(p.ctx, &alloc, &ev, 0);
    HU_ASSERT_EQ(err, HU_OK);
    hu_voice_rt_event_free(&alloc, &ev);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_openai_get_name_returns_openai_realtime(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_openai_create(&alloc, &cfg, &p);
    const char *name = p.vtable->get_name(p.ctx);
    HU_ASSERT_STR_EQ(name, "openai_realtime");
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_openai_optional_slots_have_noop_stubs(void) {
    /* OpenAI Realtime fills optional slots with no-op stubs (not NULL) */
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_openai_create(&alloc, &cfg, &p);
    HU_ASSERT_NOT_NULL(p.vtable->send_activity_start);
    HU_ASSERT_NOT_NULL(p.vtable->send_activity_end);
    HU_ASSERT_NOT_NULL(p.vtable->send_audio_stream_end);
    HU_ASSERT_NOT_NULL(p.vtable->reconnect);
    HU_ASSERT_NOT_NULL(p.vtable->send_tool_response);
    /* No-op stubs return HU_OK for signals; tool response is now a real implementation */
    HU_ASSERT_EQ(p.vtable->send_activity_start(p.ctx), HU_OK);
    HU_ASSERT_EQ(p.vtable->reconnect(p.ctx), HU_OK);
    p.vtable->connect(p.ctx);
    HU_ASSERT_EQ(p.vtable->send_tool_response(p.ctx, "test_fn", "call-1", "{}"), HU_OK);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_openai_create_null_out_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_rt_config_t cfg = {0};
    hu_error_t err = hu_voice_provider_openai_create(&alloc, &cfg, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_provider_openai_create_null_alloc_fails(void) {
    hu_voice_rt_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_error_t err = hu_voice_provider_openai_create(NULL, &cfg, &p);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* ── Gemini Live provider ─────────────────────────────────────────── */

static void voice_provider_gemini_create_connect_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    cfg.model = "test-model";
    hu_voice_provider_t p = {0};
    hu_error_t err = hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(p.vtable);
    HU_ASSERT_NOT_NULL(p.ctx);
    err = p.vtable->connect(p.ctx);
    HU_ASSERT_EQ(err, HU_OK);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_send_audio_dispatches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    unsigned char pcm[] = {0, 1, 2, 3};
    hu_error_t err = p.vtable->send_audio(p.ctx, pcm, sizeof(pcm));
    HU_ASSERT_EQ(err, HU_OK);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_recv_event_dispatches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    hu_voice_rt_event_t ev = {0};
    hu_error_t err = p.vtable->recv_event(p.ctx, &alloc, &ev, 0);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(ev.type);
    hu_voice_rt_event_free(&alloc, &ev);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_get_name_returns_gemini_live(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    const char *name = p.vtable->get_name(p.ctx);
    HU_ASSERT_STR_EQ(name, "gemini_live");
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_send_tool_response_vtable(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    HU_ASSERT_NOT_NULL(p.vtable->send_tool_response);
    if (p.vtable->send_tool_response) {
        hu_error_t err =
            p.vtable->send_tool_response(p.ctx, "get_weather", "call-001", "{\"temp\":72}");
        HU_ASSERT_EQ(err, HU_OK);
    }
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_send_tool_response_null_name_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    if (p.vtable->send_tool_response) {
        hu_error_t err = p.vtable->send_tool_response(p.ctx, NULL, "call-001", "{}");
        HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    }
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_reconnect_after_resumption_update(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    HU_ASSERT_NOT_NULL(p.vtable->reconnect);
    /* Drain mock events until sessionResumptionUpdate (seq 1) sets the handle */
    for (int i = 0; i < 2; i++) {
        hu_voice_rt_event_t ev = {0};
        p.vtable->recv_event(p.ctx, &alloc, &ev, 0);
        hu_voice_rt_event_free(&alloc, &ev);
    }
    hu_error_t err = p.vtable->reconnect(p.ctx);
    HU_ASSERT_EQ(err, HU_OK);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_reconnect_without_handle_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    p.vtable->connect(p.ctx);
    /* No events received yet — no resumption handle stored */
    hu_error_t err = p.vtable->reconnect(p.ctx);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_vad_slots_present(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_voice_provider_gemini_live_create(&alloc, &cfg, &p);
    HU_ASSERT_NOT_NULL(p.vtable->send_activity_start);
    HU_ASSERT_NOT_NULL(p.vtable->send_activity_end);
    HU_ASSERT_NOT_NULL(p.vtable->send_audio_stream_end);
    p.vtable->disconnect(p.ctx, &alloc);
}

static void voice_provider_gemini_create_null_out_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {0};
    hu_error_t err = hu_voice_provider_gemini_live_create(&alloc, &cfg, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_provider_gemini_create_null_alloc_fails(void) {
    hu_gemini_live_config_t cfg = {0};
    hu_voice_provider_t p = {0};
    hu_error_t err = hu_voice_provider_gemini_live_create(NULL, &cfg, &p);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

void run_voice_provider_tests(void) {
    HU_TEST_SUITE("voice_provider");
    HU_RUN_TEST(voice_provider_openai_create_connect_destroy);
    HU_RUN_TEST(voice_provider_openai_send_audio_dispatches);
    HU_RUN_TEST(voice_provider_openai_recv_event_dispatches);
    HU_RUN_TEST(voice_provider_openai_get_name_returns_openai_realtime);
    HU_RUN_TEST(voice_provider_openai_optional_slots_have_noop_stubs);
    HU_RUN_TEST(voice_provider_openai_create_null_out_fails);
    HU_RUN_TEST(voice_provider_openai_create_null_alloc_fails);
    HU_RUN_TEST(voice_provider_gemini_create_connect_destroy);
    HU_RUN_TEST(voice_provider_gemini_send_audio_dispatches);
    HU_RUN_TEST(voice_provider_gemini_recv_event_dispatches);
    HU_RUN_TEST(voice_provider_gemini_get_name_returns_gemini_live);
    HU_RUN_TEST(voice_provider_gemini_send_tool_response_vtable);
    HU_RUN_TEST(voice_provider_gemini_send_tool_response_null_name_fails);
    HU_RUN_TEST(voice_provider_gemini_reconnect_after_resumption_update);
    HU_RUN_TEST(voice_provider_gemini_reconnect_without_handle_fails);
    HU_RUN_TEST(voice_provider_gemini_vad_slots_present);
    HU_RUN_TEST(voice_provider_gemini_create_null_out_fails);
    HU_RUN_TEST(voice_provider_gemini_create_null_alloc_fails);
}
