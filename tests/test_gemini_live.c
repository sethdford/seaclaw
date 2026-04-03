#include "test_framework.h"
#include "human/voice/gemini_live.h"
#include "human/voice/provider.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Session lifecycle ───────────────────────────────────────────── */

static void create_session_null_alloc_fails(void) {
    hu_gemini_live_session_t *s = NULL;
    HU_ASSERT(hu_gemini_live_session_create(NULL, NULL, &s) == HU_ERR_INVALID_ARGUMENT);
}

static void create_session_null_out_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT(hu_gemini_live_session_create(&alloc, NULL, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void create_session_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .model = "gemini-3.1-flash-live-preview"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(s != NULL);
    HU_ASSERT(!s->connected);
    hu_gemini_live_session_destroy(s);
}

static void create_session_copies_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {
        .api_key = "my-key",
        .model = "gemini-3.1-flash-live-preview",
        .voice = "Kore",
        .transcribe_input = true,
        .affective_dialog = true,
    };
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(strcmp(s->config.api_key, "my-key") == 0);
    HU_ASSERT(strcmp(s->config.model, "gemini-3.1-flash-live-preview") == 0);
    HU_ASSERT(strcmp(s->config.voice, "Kore") == 0);
    HU_ASSERT(s->config.transcribe_input == true);
    HU_ASSERT(s->config.affective_dialog == true);
    hu_gemini_live_session_destroy(s);
}

/* ── Connect (test mode) ─────────────────────────────────────────── */

static void connect_test_mode_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(s->connected);
    HU_ASSERT(s->setup_sent);
    hu_gemini_live_session_destroy(s);
}

static void connect_null_session_fails(void) {
    HU_ASSERT(hu_gemini_live_connect(NULL) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Audio send ──────────────────────────────────────────────────── */

static void send_audio_test_mode_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    uint8_t pcm[320];
    memset(pcm, 0, sizeof(pcm));
    HU_ASSERT(hu_gemini_live_send_audio(s, pcm, sizeof(pcm)) == HU_OK);
    hu_gemini_live_session_destroy(s);
}

static void send_audio_null_session_fails(void) {
    uint8_t pcm[16] = {0};
    HU_ASSERT(hu_gemini_live_send_audio(NULL, pcm, sizeof(pcm)) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Text send ───────────────────────────────────────────────────── */

static void send_text_test_mode_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_send_text(s, "hello", 5) == HU_OK);
    hu_gemini_live_session_destroy(s);
}

/* ── Event receive (mock sequence) ────────────────────────────────
 * Mock sequence: 0=setupComplete, 1=sessionResumptionUpdate,
 * 2=inputTranscription, 3=audio, 4=outputTranscription,
 * 5=generationComplete, 6=turnComplete, 7=toolCall, ...
 */

static void drain_events(hu_gemini_live_session_t *s, hu_allocator_t *a, unsigned n) {
    for (unsigned i = 0; i < n; i++) {
        hu_voice_rt_event_t ev = {0};
        hu_gemini_live_recv_event(s, a, &ev, 0);
        hu_voice_rt_event_free(a, &ev);
    }
}

static void recv_event_transcript_first(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);

    /* seq 0=setupComplete, 1=sessionResumption → drain to reach seq 2 */
    drain_events(s, &alloc, 2);

    hu_voice_rt_event_t ev = {0};
    HU_ASSERT(hu_gemini_live_recv_event(s, &alloc, &ev, 100) == HU_OK);
    HU_ASSERT(strcmp(ev.type, "serverContent.inputTranscription") == 0);
    HU_ASSERT(ev.transcript != NULL);
    HU_ASSERT(strcmp(ev.transcript, "Hello from Gemini Live") == 0);
    HU_ASSERT(!ev.done);
    hu_voice_rt_event_free(&alloc, &ev);
    hu_gemini_live_session_destroy(s);
}

static void recv_event_audio_second(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);

    /* drain setupComplete + sessionResumption + inputTranscription */
    drain_events(s, &alloc, 3);

    hu_voice_rt_event_t ev = {0};
    HU_ASSERT(hu_gemini_live_recv_event(s, &alloc, &ev, 100) == HU_OK);
    HU_ASSERT(strcmp(ev.type, "serverContent.modelTurn.audio") == 0);
    HU_ASSERT(ev.audio_base64 != NULL);
    HU_ASSERT(ev.audio_base64_len > 0);
    HU_ASSERT(!ev.done);
    hu_voice_rt_event_free(&alloc, &ev);
    hu_gemini_live_session_destroy(s);
}

static void recv_event_done_third(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);

    /* drain through generationComplete (seq 0-5) to reach turnComplete (seq 6) */
    drain_events(s, &alloc, 6);

    hu_voice_rt_event_t ev = {0};
    HU_ASSERT(hu_gemini_live_recv_event(s, &alloc, &ev, 100) == HU_OK);
    HU_ASSERT(strcmp(ev.type, "serverContent.turnComplete") == 0);
    HU_ASSERT(ev.done);
    hu_voice_rt_event_free(&alloc, &ev);
    hu_gemini_live_session_destroy(s);
}

static void recv_event_disconnected_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    hu_voice_rt_event_t ev = {0};
    HU_ASSERT(hu_gemini_live_recv_event(s, &alloc, &ev, 100) == HU_ERR_IO);
    hu_gemini_live_session_destroy(s);
}

/* ── Tool support ────────────────────────────────────────────────── */

static void add_tool_test_mode_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_add_tool(s, "get_weather", "Get the weather",
                                       "{\"type\":\"object\"}") == HU_OK);
    hu_gemini_live_session_destroy(s);
}

static void send_tool_response_test_mode_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_send_tool_response(s, "get_weather", "call-123",
                                                 "{\"temp\":72}") == HU_OK);
    hu_gemini_live_session_destroy(s);
}

/* ── Manual VAD / activity signaling ──────────────────────────────── */

static void activity_start_sets_flag(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .manual_vad = true};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(!s->activity_active);
    HU_ASSERT(hu_gemini_live_send_activity_start(s) == HU_OK);
    HU_ASSERT(s->activity_active);
    hu_gemini_live_session_destroy(s);
}

static void activity_end_clears_flag(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .manual_vad = true};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_send_activity_start(s) == HU_OK);
    HU_ASSERT(s->activity_active);
    HU_ASSERT(hu_gemini_live_send_activity_end(s) == HU_OK);
    HU_ASSERT(!s->activity_active);
    hu_gemini_live_session_destroy(s);
}

static void audio_stream_end_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .manual_vad = true};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_send_audio_stream_end(s) == HU_OK);
    hu_gemini_live_session_destroy(s);
}

static void audio_gated_when_manual_vad_inactive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .manual_vad = true};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    uint8_t pcm[160] = {0};
    /* Audio blocked when activity_active is false */
    HU_ASSERT(hu_gemini_live_send_audio(s, pcm, sizeof(pcm)) == HU_ERR_IO);
    /* Audio allowed after activityStart */
    HU_ASSERT(hu_gemini_live_send_activity_start(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_send_audio(s, pcm, sizeof(pcm)) == HU_OK);
    /* Audio blocked again after activityEnd */
    HU_ASSERT(hu_gemini_live_send_activity_end(s) == HU_OK);
    HU_ASSERT(hu_gemini_live_send_audio(s, pcm, sizeof(pcm)) == HU_ERR_IO);
    hu_gemini_live_session_destroy(s);
}

static void audio_ungated_when_automatic_vad(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .manual_vad = false};
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(hu_gemini_live_connect(s) == HU_OK);
    uint8_t pcm[160] = {0};
    /* Without manual_vad, audio is always permitted */
    HU_ASSERT(hu_gemini_live_send_audio(s, pcm, sizeof(pcm)) == HU_OK);
    hu_gemini_live_session_destroy(s);
}

static void activity_null_session_fails(void) {
    HU_ASSERT(hu_gemini_live_send_activity_start(NULL) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_gemini_live_send_activity_end(NULL) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_gemini_live_send_audio_stream_end(NULL) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Config: thinking level + session resumption ─────────────────── */

static void config_thinking_level_copies(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_session_t *s = NULL;
    hu_gemini_live_config_t cfg = {
        .api_key = "test-key",
        .manual_vad = true,
        .thinking_level = HU_GL_THINKING_MINIMAL,
        .enable_session_resumption = true,
    };
    HU_ASSERT(hu_gemini_live_session_create(&alloc, &cfg, &s) == HU_OK);
    HU_ASSERT(s->config.manual_vad == true);
    HU_ASSERT(s->config.thinking_level == HU_GL_THINKING_MINIMAL);
    HU_ASSERT(s->config.enable_session_resumption == true);
    HU_ASSERT(s->resumption_handle == NULL);
    hu_gemini_live_session_destroy(s);
}

/* ── Provider vtable ─────────────────────────────────────────────── */

static void provider_create_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .model = "gemini-3.1-flash-live-preview"};
    hu_voice_provider_t vp = {0};
    HU_ASSERT(hu_voice_provider_gemini_live_create(&alloc, &cfg, &vp) == HU_OK);
    HU_ASSERT(vp.vtable != NULL);
    HU_ASSERT(vp.ctx != NULL);
    HU_ASSERT(strcmp(vp.vtable->get_name(vp.ctx), "gemini_live") == 0);
    vp.vtable->disconnect(vp.ctx, &alloc);
}

static void provider_connect_send_recv(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {.api_key = "test-key"};
    hu_voice_provider_t vp = {0};
    HU_ASSERT(hu_voice_provider_gemini_live_create(&alloc, &cfg, &vp) == HU_OK);
    HU_ASSERT(vp.vtable->connect(vp.ctx) == HU_OK);

    uint8_t pcm[160] = {0};
    HU_ASSERT(vp.vtable->send_audio(vp.ctx, pcm, sizeof(pcm)) == HU_OK);

    /* seq 0=setupComplete (no transcript), drain it */
    hu_voice_rt_event_t ev = {0};
    HU_ASSERT(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 100) == HU_OK);
    HU_ASSERT(strcmp(ev.type, "setupComplete") == 0);
    hu_voice_rt_event_free(&alloc, &ev);

    /* seq 1=sessionResumptionUpdate, drain it */
    memset(&ev, 0, sizeof(ev));
    HU_ASSERT(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 100) == HU_OK);
    hu_voice_rt_event_free(&alloc, &ev);

    /* seq 2=inputTranscription — has transcript */
    memset(&ev, 0, sizeof(ev));
    HU_ASSERT(vp.vtable->recv_event(vp.ctx, &alloc, &ev, 100) == HU_OK);
    HU_ASSERT(ev.transcript != NULL);
    hu_voice_rt_event_free(&alloc, &ev);

    vp.vtable->disconnect(vp.ctx, &alloc);
}

static void provider_null_alloc_fails(void) {
    hu_voice_provider_t vp = {0};
    HU_ASSERT(hu_voice_provider_gemini_live_create(NULL, NULL, &vp) == HU_ERR_INVALID_ARGUMENT);
}

static void provider_activity_vtable_wired(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gemini_live_config_t cfg = {.api_key = "test-key", .manual_vad = true};
    hu_voice_provider_t vp = {0};
    HU_ASSERT(hu_voice_provider_gemini_live_create(&alloc, &cfg, &vp) == HU_OK);
    HU_ASSERT(vp.vtable->send_activity_start != NULL);
    HU_ASSERT(vp.vtable->send_activity_end != NULL);
    HU_ASSERT(vp.vtable->send_audio_stream_end != NULL);
    HU_ASSERT(vp.vtable->send_activity_start(vp.ctx) == HU_OK);
    HU_ASSERT(vp.vtable->send_activity_end(vp.ctx) == HU_OK);
    HU_ASSERT(vp.vtable->send_audio_stream_end(vp.ctx) == HU_OK);
    vp.vtable->disconnect(vp.ctx, &alloc);
}

static void destroy_null_safe(void) {
    hu_gemini_live_session_destroy(NULL);
}

/* ── Test runner ─────────────────────────────────────────────────── */

void run_gemini_live_tests(void) {
    HU_TEST_SUITE("Gemini Live");

    HU_RUN_TEST(create_session_null_alloc_fails);
    HU_RUN_TEST(create_session_null_out_fails);
    HU_RUN_TEST(create_session_succeeds);
    HU_RUN_TEST(create_session_copies_config);

    HU_RUN_TEST(connect_test_mode_succeeds);
    HU_RUN_TEST(connect_null_session_fails);

    HU_RUN_TEST(send_audio_test_mode_succeeds);
    HU_RUN_TEST(send_audio_null_session_fails);

    HU_RUN_TEST(send_text_test_mode_succeeds);

    HU_RUN_TEST(recv_event_transcript_first);
    HU_RUN_TEST(recv_event_audio_second);
    HU_RUN_TEST(recv_event_done_third);
    HU_RUN_TEST(recv_event_disconnected_fails);

    HU_RUN_TEST(add_tool_test_mode_succeeds);
    HU_RUN_TEST(send_tool_response_test_mode_succeeds);

    HU_RUN_TEST(activity_start_sets_flag);
    HU_RUN_TEST(activity_end_clears_flag);
    HU_RUN_TEST(audio_stream_end_succeeds);
    HU_RUN_TEST(audio_gated_when_manual_vad_inactive);
    HU_RUN_TEST(audio_ungated_when_automatic_vad);
    HU_RUN_TEST(activity_null_session_fails);
    HU_RUN_TEST(config_thinking_level_copies);

    HU_RUN_TEST(provider_create_succeeds);
    HU_RUN_TEST(provider_connect_send_recv);
    HU_RUN_TEST(provider_null_alloc_fails);
    HU_RUN_TEST(provider_activity_vtable_wired);
    HU_RUN_TEST(destroy_null_safe);
}
