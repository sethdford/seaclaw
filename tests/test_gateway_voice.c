/* Gateway voice.transcribe routing tests (mock STT, no network). */
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/arena.h"
#include "human/core/json.h"
#include "human/gateway/control_protocol.h"
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
    const char *json =
        "{\"voice\":{\"stt_provider\":\"cartesia\"},"
        "\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}]}";
    HU_ASSERT_EQ(hu_config_parse_json(&cfg, json, strlen(json)), HU_OK);

    hu_app_context_t app = {.config = &cfg, .alloc = &backing};
    hu_control_protocol_t proto = {0};
    hu_ws_conn_t conn = {0};
    const char *req =
        "{\"method\":\"voice.transcribe\",\"params\":{\"audio\":\"ZGVzdA==\","
        "\"mimeType\":\"audio/webm\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        cp_voice_transcribe(&backing, &app, &conn, &proto, root, &out, &out_len);
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
    const char *req =
        "{\"method\":\"voice.transcribe\",\"params\":{\"audio\":\"ZGVzdA==\","
        "\"mimeType\":\"audio/webm\"}}";
    hu_json_value_t *root = NULL;
    HU_ASSERT_EQ(hu_json_parse(&backing, req, strlen(req), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        cp_voice_transcribe(&backing, &app, &conn, &proto, root, &out, &out_len);
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
    const char *json =
        "{\"providers\":[{\"name\":\"cartesia\",\"api_key\":\"ck-test\"}],"
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
    hu_error_t err =
        cp_voice_session_start(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "pcm_f32le") != NULL);
    HU_ASSERT_TRUE(strstr(out, "24000") != NULL);
    HU_ASSERT_TRUE(strstr(out, "sessionId") != NULL);
    backing.free(backing.ctx, out, out_len);
    hu_json_free(&backing, root);

    const char *req2 = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"voice.session.stop\",\"params\":{}}";
    HU_ASSERT_EQ(hu_json_parse(&backing, req2, strlen(req2), &root), HU_OK);
    out = NULL;
    out_len = 0;
    HU_ASSERT_EQ(cp_voice_session_stop(&backing, &app, &conn, &proto, root, &out, &out_len),
                 HU_OK);
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
    hu_error_t err =
        cp_voice_transcribe(&backing, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
    hu_json_free(&backing, root);
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
#endif
}
