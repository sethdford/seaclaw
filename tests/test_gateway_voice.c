/* Gateway voice control-protocol tests (mock STT, no network). */
#include "human/bus.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/arena.h"
#include "human/core/json.h"
#include "human/gateway/control_protocol.h"
#include "human/gateway/voice_stream.h"
#include "human/tool.h"
#include "test_framework.h"
#include <string.h>

#ifdef HU_GATEWAY_POSIX
#include "cp_internal.h"
#endif

#ifdef HU_GATEWAY_POSIX
static void assert_transcribe_json_text(hu_allocator_t *alloc, const char *json, size_t json_len,
                                        const char *expect_text) {
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(alloc, json, json_len, &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);
    hu_json_value_t *textv = hu_json_object_get(root, "text");
    HU_ASSERT_NOT_NULL(textv);
    HU_ASSERT_EQ(textv->type, HU_JSON_STRING);
    HU_ASSERT_STR_EQ(textv->data.string.ptr, expect_text);
    hu_json_free(alloc, root);
}

static void test_gateway_voice_transcribe_cartesia_provider(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"voice\":{\"stt_provider\":\"cartesia\"},"
                       "\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req = "{\"method\":\"voice.transcribe\",\"params\":{\"audio\":\"ZGVzdA==\","
                      "\"mimeType\":\"audio/webm\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_transcribe(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    assert_transcribe_json_text(&backing, out, out_len, "Cartesia mock transcription");
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_transcribe_default_gemini(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"gemini\",\"api_key\":\"gk-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req = "{\"method\":\"voice.transcribe\",\"params\":{\"audio\":\"ZGVzdA==\","
                      "\"mimeType\":\"audio/webm\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_transcribe(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    assert_transcribe_json_text(&backing, out, out_len, "Mock Gemini transcription");
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_session_start_returns_pcm_meta(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
                       "\"voice\":{\"tts_voice\":\"v1\",\"tts_model\":\"m1\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    conn.id = 42;
    conn.active = true;
    const char *req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.start\","
                      "\"params\":{\"voiceId\":\"vid\",\"modelId\":\"mid\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "pcm_f32le") != NULL);
    HU_ASSERT_TRUE(strstr(out, "24000") != NULL);
    HU_ASSERT_TRUE(strstr(out, "sessionId") != NULL);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    const char *req2 =
        "{\"type\":\"req\",\"id\":\"2\",\"method\":\"voice.session.stop\",\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, req2, strlen(req2), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_stop(&backing, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    hu_arena_destroy(arena);
}

static void test_gateway_voice_transcribe_missing_audio_returns_error(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"gemini\",\"api_key\":\"gk-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req = "{\"method\":\"voice.transcribe\",\"params\":{}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_transcribe(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}
static void test_gateway_voice_session_interrupt_clears_pcm(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
                       "\"voice\":{\"tts_voice\":\"v1\",\"tts_model\":\"m1\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    conn.id = 50;
    conn.active = true;

    /* Start session */
    const char *start_req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.start\","
                            "\"params\":{}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, start_req, strlen(start_req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* Feed binary data */
    const char audio[] = "fake-audio-data";
    hu_voice_stream_on_binary(&proto, &conn, audio, sizeof(audio));

    /* Interrupt */
    const char *int_req = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"voice.session.interrupt\","
                          "\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, int_req, strlen(int_req), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_interrupt(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "true") != NULL);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* Stop to clean up slot */
    const char *stop_req = "{\"type\":\"req\",\"id\":\"3\",\"method\":\"voice.session.stop\","
                           "\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, stop_req, strlen(stop_req), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_stop(&backing, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_on_binary_accumulates(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
                       "\"voice\":{\"tts_voice\":\"v1\",\"tts_model\":\"m1\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    proto.alloc = &backing;
    hu_ws_conn_t conn = {0};
    conn.id = 60;
    conn.active = true;

    /* Start session to allocate a slot */
    const char *start_req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.start\","
                            "\"params\":{}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, start_req, strlen(start_req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* Send two binary chunks */
    const char chunk1[] = "AAAA";
    const char chunk2[] = "BBBB";
    hu_voice_stream_on_binary(&proto, &conn, chunk1, 4);
    hu_voice_stream_on_binary(&proto, &conn, chunk2, 4);

    /* Without a conn slot, binary should be silently ignored */
    hu_ws_conn_t bogus = {0};
    bogus.id = 999;
    hu_voice_stream_on_binary(&proto, &bogus, chunk1, 4);

    /* Null/zero-length should be ignored without crash */
    hu_voice_stream_on_binary(&proto, &conn, NULL, 0);
    hu_voice_stream_on_binary(&proto, &conn, chunk1, 0);

    /* Stop to clean up */
    const char *stop_req = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"voice.session.stop\","
                           "\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, stop_req, strlen(stop_req), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_stop(&backing, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_on_conn_close_frees_slot(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
                       "\"voice\":{\"tts_voice\":\"v1\",\"tts_model\":\"m1\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    proto.alloc = &backing;
    hu_ws_conn_t conn = {0};
    conn.id = 70;
    conn.active = true;

    /* Start session */
    const char *start_req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.start\","
                            "\"params\":{}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, start_req, strlen(start_req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* Simulate disconnect — should free the slot without crash */
    hu_voice_stream_on_conn_close(&conn);

    /* Calling again on same conn should be harmless (slot already freed) */
    hu_voice_stream_on_conn_close(&conn);

    /* A NULL conn should be harmless */
    hu_voice_stream_on_conn_close(NULL);

    hu_arena_destroy(arena);
}

static void test_gateway_voice_audio_end_no_data_returns_error(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
                       "\"voice\":{\"tts_voice\":\"v1\",\"tts_model\":\"m1\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_bus_t bus;
    hu_bus_init(&bus);
    hu_app_context_t app = {.config = &cfg, .alloc = &backing, .bus = &bus};
    hu_control_protocol_t proto = {0};
    proto.alloc = &backing;
    hu_ws_conn_t conn = {0};
    conn.id = 80;
    conn.active = true;

    /* Start session */
    const char *start_req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.start\","
                            "\"params\":{}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, start_req, strlen(start_req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* audio.end without any binary data should fail */
    const char *end_req = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"voice.audio.end\","
                          "\"params\":{\"mimeType\":\"audio/webm\"}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, end_req, strlen(end_req), &root), HU_OK);
    out = NULL;
    out_len = 0;
    hu_error_t err = cp_voice_audio_end(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    hu_json_free(&backing, root);

    /* Clean up */
    hu_voice_stream_on_conn_close(&conn);
    hu_bus_deinit(&bus);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_double_start_reuses_slot(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
                       "\"voice\":{\"tts_voice\":\"v1\",\"tts_model\":\"m1\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    conn.id = 90;
    conn.active = true;

    const char *start_req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.start\","
                            "\"params\":{}}";
    hu_json_value_t *root = NULL;
    char *out = NULL;
    size_t out_len = 0;

    /* First start */
    HU_ASSERT_EQ(hu_json_parse(&backing, start_req, strlen(start_req), &root), HU_OK);
    HU_ASSERT_EQ(cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* Second start on same conn — should succeed (reuse slot) */
    HU_ASSERT_EQ(hu_json_parse(&backing, start_req, strlen(start_req), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    /* Clean up */
    const char *stop_req = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"voice.session.stop\","
                           "\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, stop_req, strlen(stop_req), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_stop(&backing, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_interrupt_without_session_returns_ok(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    conn.id = 100;
    conn.active = true;

    /* Interrupt without a session — should return ok (no slot to interrupt) */
    const char *int_req = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"voice.session.interrupt\","
                          "\"params\":{}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, int_req, strlen(int_req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_interrupt(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "true") != NULL);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}
static void test_gateway_voice_clone_returns_voice_id(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req = "{\"method\":\"voice.clone\",\"params\":{\"audio\":\"ZGVzdA==\","
                      "\"mimeType\":\"audio/wav\",\"name\":\"Test Voice\",\"language\":\"en\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_clone(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "voice_id") != NULL);
    HU_ASSERT_TRUE(strstr(out, "Test Voice") != NULL);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_clone_missing_audio_returns_error(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req = "{\"method\":\"voice.clone\",\"params\":{\"name\":\"Test\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_clone(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_clone_missing_params_returns_error(void) {
    hu_allocator_t backing = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(backing);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *json = "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req = "{\"method\":\"voice.clone\"}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_voice_clone(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_json_free(&backing, root);
    hu_arena_destroy(arena);
}

static void test_gateway_voice_clone_null_alloc_returns_error(void) {
    hu_error_t err = cp_voice_clone(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_gl_session_start_returns_mode(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);
    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {.id = 200, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "gemini_live") != NULL);
    HU_ASSERT_TRUE(strstr(out, "pcm_f32le") != NULL);
    HU_ASSERT_TRUE(strstr(out, "inputSampleRate") != NULL);
    HU_ASSERT_TRUE(strstr(out, "outputSampleRate") != NULL);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

static void test_gl_audio_end_ok(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);
    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {.id = 201, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    int16_t pcm[160];
    memset(pcm, 0, sizeof(pcm));
    hu_voice_stream_on_binary(&proto, &conn, (const char *)pcm, sizeof(pcm));
    const char *req2 = "{\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&b, req2, strlen(req2), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_audio_end(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "ok") != NULL);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

/* cp_voice_tool_response is implemented in cp_voice_stream.c */

static void test_gl_tool_response_null_fails(void) {
    HU_ASSERT_EQ(cp_voice_tool_response(NULL, NULL, NULL, NULL, NULL, NULL, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_gl_tool_response_ok(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);
    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {.id = 202, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    const char *req2 = "{\"params\":{\"callId\":\"c1\",\"name\":\"weather\",\"result\":\"{}\"}}";
    HU_ASSERT_EQ(hu_json_parse(&b, req2, strlen(req2), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_tool_response(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "ok") != NULL);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

static void test_gl_interrupt_resets_activity(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);
    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {.id = 203, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    int16_t pcm[80];
    memset(pcm, 0, sizeof(pcm));
    hu_voice_stream_on_binary(&proto, &conn, (const char *)pcm, sizeof(pcm));
    const char *req2 = "{\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&b, req2, strlen(req2), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_interrupt(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "ok") != NULL);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

static void test_gl_poll_processes_events(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);
    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {.alloc = &b};
    hu_ws_conn_t conn = {.id = 300, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);
    /* Poll drains mock events (audio, transcript, toolCall, etc.) without crashing.
       s_proto->ws is NULL so send_event_to_conn is a no-op, but the drain loop runs. */
    hu_voice_stream_poll_gemini_live();
    hu_voice_stream_poll_gemini_live();
    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

/* ── Tool execution in poll loop with registered tools ──────────── */

static const char *mock_tool_name(void *ctx) {
    (void)ctx;
    return "get_weather";
}

static const char *mock_tool_desc(void *ctx) {
    (void)ctx;
    return "Get weather";
}

static const char *mock_tool_params(void *ctx) {
    (void)ctx;
    return "{\"type\":\"object\"}";
}

static hu_error_t mock_tool_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                    hu_tool_result_t *out) {
    (void)ctx;
    (void)alloc;
    (void)args;
    *out = hu_tool_result_ok("{\"temp\":72}", 11);
    return HU_OK;
}

static const hu_tool_vtable_t mock_tool_vtable = {
    .name = mock_tool_name,
    .description = mock_tool_desc,
    .parameters_json = mock_tool_params,
    .execute = mock_tool_execute,
};

static void test_gl_poll_executes_tool_calls(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);

    hu_tool_t tools[1];
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_tool_vtable;

    hu_app_context_t app = {.config = &cfg, .alloc = &b, .tools = tools, .tools_count = 1};
    hu_control_protocol_t proto = {.alloc = &b, .app_ctx = &app};
    hu_ws_conn_t conn = {.id = 400, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);

    /* Poll multiple times — mock seq 7 is toolCall with name "get_weather".
     * Since ws is NULL, send_event_to_conn is a no-op, but tool execution path runs.
     * The tool result is sent back through send_tool_response vtable. */
    hu_voice_stream_poll_gemini_live();
    hu_voice_stream_poll_gemini_live();

    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

/* ── goAway reconnect via poll loop ────────────────────────────── */

static void test_gl_poll_reconnect_on_goaway(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {.alloc = &b, .app_ctx = &app};
    hu_ws_conn_t conn = {.id = 500, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);

    /* The mock event sequence:
     *   0=setupComplete, 1=sessionResumptionUpdate (sets handle),
     *   2=inputTranscription, 3=audio, 4=outputTranscription,
     *   5=generationComplete, 6=turnComplete(done), 7=toolCall,
     *   8=toolCallCancellation, 9=goAway(30000ms)
     *
     * After goAway at seq 9, reconnect should succeed (handle was set at seq 1).
     * Each poll drains up to 16 events. The first poll drains seq 0-6 (stops at done).
     * Second poll: seq 7. Third poll: seq 8. Fourth poll: seq 9 (goAway → reconnect).
     * After reconnect the session should still be usable. */
    hu_voice_stream_poll_gemini_live(); /* seq 0-6 (turnComplete=done → break) */
    hu_voice_stream_poll_gemini_live(); /* seq 7 (toolCall) */
    hu_voice_stream_poll_gemini_live(); /* seq 8 (toolCallCancellation) */
    hu_voice_stream_poll_gemini_live(); /* seq 9 (goAway → reconnect) */

    /* Session should still be alive — poll one more time to verify no crash. */
    hu_voice_stream_poll_gemini_live();

    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

/* ── Full lifecycle: start → binary → poll → interrupt → stop ──── */

static void test_gl_full_lifecycle(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {.alloc = &b, .app_ctx = &app};
    hu_ws_conn_t conn = {.id = 600, .active = true};

    /* Start */
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "gemini_live") != NULL);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);

    /* Binary audio */
    int16_t pcm[160];
    memset(pcm, 0, sizeof(pcm));
    hu_voice_stream_on_binary(&proto, &conn, (const char *)pcm, sizeof(pcm));

    /* Poll */
    hu_voice_stream_poll_gemini_live();

    /* Interrupt */
    const char *ireq = "{\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&b, ireq, strlen(ireq), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_interrupt(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);

    /* Stop */
    const char *sreq = "{\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&b, sreq, strlen(sreq), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_stop(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);

    hu_arena_destroy(arena);
}

/* ── Tool response with missing params ─────────────────────────── */

static void test_gl_tool_response_missing_name_fails(void) {
    hu_allocator_t b = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_arena_t *arena = hu_arena_create(b);
    HU_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = hu_arena_allocator(arena);
    const char *j = "{\"providers\":[{\"name\":\"google\",\"api_key\":\"gk\"}],"
                    "\"voice\":{\"mode\":\"gemini_live\"}}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, j, strlen(j)), HU_OK);
    hu_app_context_t app = {.config = &cfg, .alloc = &b};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {.id = 700, .active = true};
    const char *req = "{\"params\":{\"mode\":\"gemini_live\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&b, req, strlen(req), &root), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_start(&b, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    if (out)
        b.free(b.ctx, out, out_len);
    hu_json_free(&b, root);

    /* Missing name — should fail */
    const char *req2 = "{\"params\":{\"callId\":\"c1\",\"result\":\"{}\"}}";
    HU_ASSERT_EQ(hu_json_parse(&b, req2, strlen(req2), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_tool_response(&b, &app, &conn, &proto, root, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    hu_json_free(&b, root);

    hu_voice_stream_on_conn_close(&conn);
    hu_arena_destroy(arena);
}

#endif /* HU_GATEWAY_POSIX */

void run_gateway_voice_tests(void) {
#ifdef HU_GATEWAY_POSIX
    HU_TEST_SUITE("Gateway voice");
    HU_RUN_TEST(test_gateway_voice_transcribe_cartesia_provider);
    HU_RUN_TEST(test_gateway_voice_transcribe_default_gemini);
    HU_RUN_TEST(test_gateway_voice_session_start_returns_pcm_meta);
    HU_RUN_TEST(test_gateway_voice_transcribe_missing_audio_returns_error);
    HU_RUN_TEST(test_gateway_voice_session_interrupt_clears_pcm);
    HU_RUN_TEST(test_gateway_voice_on_binary_accumulates);
    HU_RUN_TEST(test_gateway_voice_on_conn_close_frees_slot);
    HU_RUN_TEST(test_gateway_voice_audio_end_no_data_returns_error);
    HU_RUN_TEST(test_gateway_voice_double_start_reuses_slot);
    HU_RUN_TEST(test_gateway_voice_interrupt_without_session_returns_ok);
    HU_RUN_TEST(test_gateway_voice_clone_returns_voice_id);
    HU_RUN_TEST(test_gateway_voice_clone_missing_audio_returns_error);
    HU_RUN_TEST(test_gateway_voice_clone_missing_params_returns_error);
    HU_RUN_TEST(test_gateway_voice_clone_null_alloc_returns_error);
    HU_RUN_TEST(test_gl_session_start_returns_mode);
    HU_RUN_TEST(test_gl_audio_end_ok);
    HU_RUN_TEST(test_gl_tool_response_null_fails);
    HU_RUN_TEST(test_gl_tool_response_ok);
    HU_RUN_TEST(test_gl_interrupt_resets_activity);
    HU_RUN_TEST(test_gl_poll_processes_events);
    HU_RUN_TEST(test_gl_poll_executes_tool_calls);
    HU_RUN_TEST(test_gl_poll_reconnect_on_goaway);
    HU_RUN_TEST(test_gl_full_lifecycle);
    HU_RUN_TEST(test_gl_tool_response_missing_name_fails);
#endif
}
