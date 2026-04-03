#include "human/voice/gemini_live.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/multimodal.h"
#include "human/voice/provider.h"
#include "human/websocket/websocket.h"
#include <stdio.h>
#include <string.h>

/* ── Endpoint construction ────────────────────────────────────────── */

static const char *DEFAULT_MODEL = "gemini-3.1-flash-live-preview";
static const char *DEFAULT_VOICE = "Puck";

#if !HU_IS_TEST
static const char *GOOGLE_AI_WS_BASE =
    "wss://generativelanguage.googleapis.com/ws/"
    "google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent";
#endif

/*
 * Build WebSocket URL.
 * Google AI: ...?key=API_KEY
 * Vertex AI: wss://{region}-aiplatform.googleapis.com/ws/...?access_token=TOKEN
 */
#if !HU_IS_TEST
static int build_ws_url(const hu_gemini_live_config_t *cfg, char *buf, size_t cap) {
    if (cfg->region && cfg->region[0] && cfg->project_id && cfg->project_id[0]) {
        const char *token = cfg->access_token ? cfg->access_token : "";
        return snprintf(buf, cap,
                        "wss://%s-aiplatform.googleapis.com/ws/"
                        "google.cloud.aiplatform.v1beta1.LlmBidiService/"
                        "BidiGenerateContent?access_token=%s",
                        cfg->region, token);
    }
    const char *key = cfg->api_key ? cfg->api_key : "";
    return snprintf(buf, cap, "%s?key=%s", GOOGLE_AI_WS_BASE, key);
}
#endif

/* ── Setup JSON builder ──────────────────────────────────────────── */

/*
 * Build BidiGenerateContentSetup per the Live API spec:
 * https://ai.google.dev/api/live#BidiGenerateContentSetup
 *
 * Envelope: {"setup": { ... }}
 * generationConfig contains: responseModalities, speechConfig, thinkingConfig
 * Top-level setup fields: model, realtimeInputConfig, sessionResumption,
 *                         systemInstruction, inputAudioTranscription,
 *                         outputAudioTranscription
 */
hu_error_t hu_gemini_live_build_setup_json(hu_allocator_t *alloc,
                                           const hu_gemini_live_config_t *config,
                                           const char *resumption_handle, char **out_json,
                                           size_t *out_len) {
    if (!alloc || !config || !out_json || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    const char *model = (config->model && config->model[0]) ? config->model : DEFAULT_MODEL;
    const char *voice = (config->voice && config->voice[0]) ? config->voice : DEFAULT_VOICE;
    const char *sys = config->system_instruction;

    hu_json_buf_t buf = {0};
    hu_error_t err = hu_json_buf_init(&buf, alloc);
    if (err != HU_OK)
        return err;

    err = hu_json_buf_append_raw(&buf, "{\"setup\":{", 10);
    if (err != HU_OK)
        goto fail;

    /* model (Vertex AI = full resource name; Google AI = models/ prefix) */
    if (config->region && config->region[0]) {
        char mp[256];
        int n = snprintf(mp, sizeof(mp), "projects/%s/locations/%s/publishers/google/models/%s",
                         config->project_id ? config->project_id : "", config->region, model);
        if (n > 0 && (size_t)n < sizeof(mp)) {
            err = hu_json_buf_append_raw(&buf, "\"model\":\"", 9);
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, mp, (size_t)n);
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, "\",", 2);
        }
    } else {
        char mp[256];
        int n = snprintf(mp, sizeof(mp), "models/%s", model);
        if (n > 0 && (size_t)n < sizeof(mp)) {
            err = hu_json_buf_append_raw(&buf, "\"model\":\"", 9);
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, mp, (size_t)n);
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, "\",", 2);
        }
    }
    if (err != HU_OK)
        goto fail;

    /* generationConfig: responseModalities, speechConfig, thinkingConfig */
    static const char gc_prefix[] =
        "\"generationConfig\":{\"responseModalities\":[\"AUDIO\"],"
        "\"speechConfig\":{\"voiceConfig\":{\"prebuiltVoiceConfig\":{\"voiceName\":\"";
    err = hu_json_buf_append_raw(&buf, gc_prefix, sizeof(gc_prefix) - 1);
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, voice, strlen(voice));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, "\"}}}", 4);
    if (err != HU_OK)
        goto fail;

    /* thinkingConfig inside generationConfig */
    if (config->thinking_level != HU_GL_THINKING_DEFAULT) {
        static const char *level_strings[] = {
            [HU_GL_THINKING_NONE] = "none", [HU_GL_THINKING_MINIMAL] = "minimal",
            [HU_GL_THINKING_LOW] = "low",   [HU_GL_THINKING_MEDIUM] = "medium",
            [HU_GL_THINKING_HIGH] = "high",
        };
        const char *lvl = level_strings[config->thinking_level];
        err = hu_json_buf_append_raw(&buf, ",\"thinkingConfig\":{\"thinkingLevel\":\"", 36);
        if (err == HU_OK)
            err = hu_json_buf_append_raw(&buf, lvl, strlen(lvl));
        if (err == HU_OK)
            err = hu_json_buf_append_raw(&buf, "\"}", 2);
        if (err != HU_OK)
            goto fail;
    }

    /* close generationConfig */
    err = hu_json_buf_append_raw(&buf, "}", 1);
    if (err != HU_OK)
        goto fail;

    /* realtimeInputConfig (setup-level, not inside generationConfig) */
    if (config->manual_vad) {
        static const char vad_str[] =
            ",\"realtimeInputConfig\":"
            "{\"automaticActivityDetection\":{\"disabled\":true}}";
        err = hu_json_buf_append_raw(&buf, vad_str, sizeof(vad_str) - 1);
        if (err != HU_OK)
            goto fail;
    }

    /* sessionResumption (setup-level) */
    if (config->enable_session_resumption || (resumption_handle && resumption_handle[0])) {
        err = hu_json_buf_append_raw(&buf, ",\"sessionResumption\":{\"handle\":\"", 32);
        if (err == HU_OK && resumption_handle && resumption_handle[0])
            err = hu_json_buf_append_raw(&buf, resumption_handle, strlen(resumption_handle));
        if (err == HU_OK)
            err = hu_json_buf_append_raw(&buf, "\"}", 2);
        if (err != HU_OK)
            goto fail;
    }

    /* inputAudioTranscription (setup-level) */
    if (config->transcribe_input) {
        err = hu_json_buf_append_raw(&buf, ",\"inputAudioTranscription\":{}", 29);
        if (err != HU_OK)
            goto fail;
    }

    /* outputAudioTranscription (setup-level) */
    if (config->transcribe_output) {
        err = hu_json_buf_append_raw(&buf, ",\"outputAudioTranscription\":{}", 30);
        if (err != HU_OK)
            goto fail;
    }

    /* systemInstruction (setup-level) — use escaped append for safe JSON */
    if (sys && sys[0]) {
        err = hu_json_buf_append_raw(&buf, ",\"systemInstruction\":{\"parts\":[{\"text\":", 39);
        if (err == HU_OK)
            err = hu_json_append_string(&buf, sys, strlen(sys));
        if (err == HU_OK)
            err = hu_json_buf_append_raw(&buf, "}]}", 3);
        if (err != HU_OK)
            goto fail;
    }

    /* tools (setup-level) — pre-built JSON array of functionDeclarations */
    if (config->tools_json && config->tools_json[0]) {
        err = hu_json_buf_append_raw(&buf, ",\"tools\":[{\"functionDeclarations\":", 33);
        if (err == HU_OK)
            err = hu_json_buf_append_raw(&buf, config->tools_json, strlen(config->tools_json));
        if (err == HU_OK)
            err = hu_json_buf_append_raw(&buf, "}]", 2);
        if (err != HU_OK)
            goto fail;
    }

    /* close setup + envelope */
    err = hu_json_buf_append_raw(&buf, "}}", 2);
    if (err != HU_OK)
        goto fail;

    *out_json = buf.ptr;
    *out_len = buf.len;
    return HU_OK;

fail:
    hu_json_buf_free(&buf);
    return err;
}

/* ── Session lifecycle ───────────────────────────────────────────── */

hu_error_t hu_gemini_live_session_create(hu_allocator_t *alloc,
                                         const hu_gemini_live_config_t *config,
                                         hu_gemini_live_session_t **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    hu_gemini_live_session_t *s = alloc->alloc(alloc->ctx, sizeof(*s));
    if (!s)
        return HU_ERR_OUT_OF_MEMORY;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    if (config)
        s->config = *config;
#if HU_IS_TEST
    s->test_recv_seq = 0;
#endif
    *out = s;
    return HU_OK;
}

hu_error_t hu_gemini_live_connect(hu_gemini_live_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
#if HU_IS_TEST
    session->connected = true;
    session->setup_sent = true;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    char url[1024];
    int n = build_ws_url(&session->config, url, sizeof(url));
    if (n <= 0 || (size_t)n >= sizeof(url))
        return HU_ERR_INVALID_ARGUMENT;

    hu_ws_client_t *ws = NULL;
    hu_error_t err = hu_ws_connect(session->alloc, url, &ws);
    if (err != HU_OK || !ws)
        return err;
    session->ws_client = ws;
    session->connected = true;

    char *setup_json = NULL;
    size_t setup_len = 0;
    err = hu_gemini_live_build_setup_json(session->alloc, &session->config, NULL, &setup_json,
                                          &setup_len);
    if (err != HU_OK) {
        hu_ws_client_free(ws, session->alloc);
        session->ws_client = NULL;
        session->connected = false;
        return err;
    }

    err = hu_ws_send(ws, setup_json, setup_len);
    session->alloc->free(session->alloc->ctx, setup_json, setup_len + 1);
    if (err != HU_OK) {
        hu_ws_client_free(ws, session->alloc);
        session->ws_client = NULL;
        session->connected = false;
        return err;
    }
    session->setup_sent = true;
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_gemini_live_reconnect(hu_gemini_live_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    if (!session->resumption_handle || !session->resumption_handle[0])
        return HU_ERR_NOT_FOUND;

#if HU_IS_TEST
    session->connected = true;
    session->setup_sent = true;
    session->activity_active = false;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    /* Close existing connection */
    if (session->ws_client) {
        hu_ws_client_free((hu_ws_client_t *)session->ws_client, session->alloc);
        session->ws_client = NULL;
    }
    session->connected = false;
    session->setup_sent = false;
    session->activity_active = false;

    /* Reconnect WebSocket */
    char url[1024];
    int n = build_ws_url(&session->config, url, sizeof(url));
    if (n <= 0 || (size_t)n >= sizeof(url))
        return HU_ERR_INVALID_ARGUMENT;

    hu_ws_client_t *ws = NULL;
    hu_error_t err = hu_ws_connect(session->alloc, url, &ws);
    if (err != HU_OK || !ws)
        return err;
    session->ws_client = ws;
    session->connected = true;

    /* Send setup with resumption handle */
    char *setup_json = NULL;
    size_t setup_len = 0;
    err = hu_gemini_live_build_setup_json(session->alloc, &session->config,
                                          session->resumption_handle, &setup_json, &setup_len);
    if (err != HU_OK) {
        hu_ws_client_free(ws, session->alloc);
        session->ws_client = NULL;
        session->connected = false;
        return err;
    }

    err = hu_ws_send(ws, setup_json, setup_len);
    session->alloc->free(session->alloc->ctx, setup_json, setup_len + 1);
    if (err != HU_OK) {
        hu_ws_client_free(ws, session->alloc);
        session->ws_client = NULL;
        session->connected = false;
        return err;
    }
    session->setup_sent = true;
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
}

/* ── Activity signaling (manual VAD) ─────────────────────────────── */

static hu_error_t gl_send_simple_msg(hu_gemini_live_session_t *session, const char *json,
                                     size_t json_len) {
#if HU_IS_TEST
    (void)session;
    (void)json;
    (void)json_len;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;
    return hu_ws_send((hu_ws_client_t *)session->ws_client, json, json_len);
#else
    (void)session;
    (void)json;
    (void)json_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_gemini_live_send_activity_start(hu_gemini_live_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    static const char msg[] = "{\"realtimeInput\":{\"activityStart\":{}}}";
    hu_error_t err = gl_send_simple_msg(session, msg, sizeof(msg) - 1);
    if (err == HU_OK)
        session->activity_active = true;
    return err;
}

hu_error_t hu_gemini_live_send_activity_end(hu_gemini_live_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    static const char msg[] = "{\"realtimeInput\":{\"activityEnd\":{}}}";
    hu_error_t err = gl_send_simple_msg(session, msg, sizeof(msg) - 1);
    if (err == HU_OK)
        session->activity_active = false;
    return err;
}

hu_error_t hu_gemini_live_send_audio_stream_end(hu_gemini_live_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    static const char msg[] = "{\"realtimeInput\":{\"audioStreamEnd\":true}}";
    return gl_send_simple_msg(session, msg, sizeof(msg) - 1);
}

/* ── Audio send ──────────────────────────────────────────────────── */

hu_error_t hu_gemini_live_send_audio(hu_gemini_live_session_t *session, const void *pcm16,
                                     size_t len) {
    if (!session || !pcm16)
        return HU_ERR_INVALID_ARGUMENT;
    /* In manual VAD mode, block audio between activityEnd and next activityStart.
     * Sending audio in this gap causes 1007 (precondition failed) disconnects. */
    if (session->config.manual_vad && !session->activity_active)
        return HU_ERR_IO;
#if HU_IS_TEST
    (void)len;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;

    char *b64 = NULL;
    size_t b64_len = 0;
    hu_error_t err = hu_multimodal_encode_base64(session->alloc, pcm16, len, &b64, &b64_len);
    if (err != HU_OK || !b64)
        return err;

    int rate = session->config.sample_rate_in > 0 ? session->config.sample_rate_in : 16000;
    size_t json_cap = 128 + b64_len;
    char *json = (char *)session->alloc->alloc(session->alloc->ctx, json_cap);
    if (!json) {
        session->alloc->free(session->alloc->ctx, b64, b64_len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    int n = snprintf(json, json_cap,
                     "{\"realtimeInput\":{\"audio\":{\"data\":\"%.*s\","
                     "\"mimeType\":\"audio/pcm;rate=%d\"}}}",
                     (int)b64_len, b64, rate);
    session->alloc->free(session->alloc->ctx, b64, b64_len + 1);
    if (n <= 0 || (size_t)n >= json_cap) {
        session->alloc->free(session->alloc->ctx, json, json_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }
    err = hu_ws_send((hu_ws_client_t *)session->ws_client, json, (size_t)n);
    session->alloc->free(session->alloc->ctx, json, json_cap);
    return err;
#else
    (void)len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

/* ── Text send ───────────────────────────────────────────────────── */

hu_error_t hu_gemini_live_send_text(hu_gemini_live_session_t *session, const char *text,
                                    size_t text_len) {
    if (!session || !text)
        return HU_ERR_INVALID_ARGUMENT;
#if HU_IS_TEST
    (void)text_len;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;

    size_t json_cap = 64 + text_len * 2;
    char *json = (char *)session->alloc->alloc(session->alloc->ctx, json_cap);
    if (!json)
        return HU_ERR_OUT_OF_MEMORY;
    int n =
        snprintf(json, json_cap, "{\"realtimeInput\":{\"text\":\"%.*s\"}}", (int)text_len, text);
    if (n <= 0 || (size_t)n >= json_cap) {
        session->alloc->free(session->alloc->ctx, json, json_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }
    hu_error_t err = hu_ws_send((hu_ws_client_t *)session->ws_client, json, (size_t)n);
    session->alloc->free(session->alloc->ctx, json, json_cap);
    return err;
#else
    (void)text_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

/* ── Event receive ───────────────────────────────────────────────── */

#if HU_IS_TEST || defined(HU_HTTP_CURL)
static void gl_copy_event_type(const char *type, hu_voice_rt_event_t *out) {
    if (type) {
        strncpy(out->type, type, sizeof(out->type) - 1);
        out->type[sizeof(out->type) - 1] = '\0';
    }
}
#endif

/*
 * Mock event sequence for HU_IS_TEST mode.
 * Simulates a realistic Gemini Live conversation:
 *   0: setupComplete
 *   1: sessionResumptionUpdate
 *   2: inputTranscription
 *   3: modelTurn audio
 *   4: outputTranscription
 *   5: generationComplete
 *   6: turnComplete
 *   7: toolCall
 *   8: toolCallCancellation
 *   9: goAway
 *  10: interrupted + turnComplete
 *  11+: wraps to audio deltas
 */
hu_error_t hu_gemini_live_recv_event(hu_gemini_live_session_t *session, hu_allocator_t *alloc,
                                     hu_voice_rt_event_t *out, int timeout_ms) {
    if (!session || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
#if HU_IS_TEST
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;
    (void)timeout_ms;
    if (!session->connected)
        return HU_ERR_IO;
    unsigned call = session->test_recv_seq++;
    switch (call) {
    case 0:
        gl_copy_event_type("setupComplete", out);
        break;
    case 1: {
        gl_copy_event_type("sessionResumptionUpdate", out);
        static const char k_handle[] = "test-resumption-handle-abc123";
        if (session->resumption_handle) {
            session->alloc->free(session->alloc->ctx, session->resumption_handle,
                                 session->resumption_handle_len + 1);
        }
        session->resumption_handle = hu_strndup(session->alloc, k_handle, sizeof(k_handle) - 1);
        session->resumption_handle_len = sizeof(k_handle) - 1;
        break;
    }
    case 2: {
        gl_copy_event_type("serverContent.inputTranscription", out);
        static const char k_text[] = "Hello from Gemini Live";
        out->transcript = hu_strndup(alloc, k_text, sizeof(k_text) - 1);
        if (!out->transcript)
            return HU_ERR_OUT_OF_MEMORY;
        out->transcript_len = sizeof(k_text) - 1;
        break;
    }
    case 3: {
        gl_copy_event_type("serverContent.modelTurn.audio", out);
        static const char k_b64[] = "AAAA";
        out->audio_base64 = hu_strndup(alloc, k_b64, sizeof(k_b64) - 1);
        if (!out->audio_base64)
            return HU_ERR_OUT_OF_MEMORY;
        out->audio_base64_len = sizeof(k_b64) - 1;
        break;
    }
    case 4: {
        gl_copy_event_type("serverContent.outputTranscription", out);
        static const char k_text[] = "Hi there, how can I help?";
        out->transcript = hu_strndup(alloc, k_text, sizeof(k_text) - 1);
        if (!out->transcript)
            return HU_ERR_OUT_OF_MEMORY;
        out->transcript_len = sizeof(k_text) - 1;
        break;
    }
    case 5:
        gl_copy_event_type("serverContent.generationComplete", out);
        out->generation_complete = true;
        break;
    case 6:
        gl_copy_event_type("serverContent.turnComplete", out);
        out->done = true;
        break;
    case 7: {
        gl_copy_event_type("toolCall", out);
        static const char k_name[] = "get_weather";
        static const char k_id[] = "call-001";
        static const char k_args[] = "{\"location\":\"San Francisco\"}";
        out->transcript = hu_strndup(alloc, k_name, sizeof(k_name) - 1);
        if (!out->transcript)
            return HU_ERR_OUT_OF_MEMORY;
        out->transcript_len = sizeof(k_name) - 1;
        out->tool_call_id = hu_strndup(alloc, k_id, sizeof(k_id) - 1);
        if (!out->tool_call_id)
            return HU_ERR_OUT_OF_MEMORY;
        out->tool_call_id_len = sizeof(k_id) - 1;
        out->tool_args_json = hu_strndup(alloc, k_args, sizeof(k_args) - 1);
        if (!out->tool_args_json)
            return HU_ERR_OUT_OF_MEMORY;
        out->tool_args_json_len = sizeof(k_args) - 1;
        break;
    }
    case 8: {
        gl_copy_event_type("toolCallCancellation", out);
        static const char k_ids[] = "call-001,call-002";
        out->transcript = hu_strndup(alloc, k_ids, sizeof(k_ids) - 1);
        if (!out->transcript)
            return HU_ERR_OUT_OF_MEMORY;
        out->transcript_len = sizeof(k_ids) - 1;
        break;
    }
    case 9:
        gl_copy_event_type("goAway", out);
        out->go_away_ms = 30000;
        break;
    case 10:
        gl_copy_event_type("serverContent.interrupted", out);
        out->interrupted = true;
        out->done = true;
        break;
    default: {
        gl_copy_event_type("serverContent.modelTurn.audio", out);
        static const char k_b64[] = "AQID";
        out->audio_base64 = hu_strndup(alloc, k_b64, sizeof(k_b64) - 1);
        if (!out->audio_base64)
            return HU_ERR_OUT_OF_MEMORY;
        out->audio_base64_len = sizeof(k_b64) - 1;
        break;
    }
    }
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;

    char *msg = NULL;
    size_t msg_len = 0;
    hu_error_t err =
        hu_ws_recv((hu_ws_client_t *)session->ws_client, alloc, &msg, &msg_len, timeout_ms);
    if (err != HU_OK || !msg) {
        if (msg)
            alloc->free(alloc->ctx, msg, msg_len + 1);
        return err;
    }

    hu_json_value_t *json = NULL;
    err = hu_json_parse(alloc, msg, msg_len, &json);
    if (err != HU_OK || !json) {
        alloc->free(alloc->ctx, msg, msg_len + 1);
        return err != HU_OK ? err : HU_ERR_IO;
    }

    /* ── serverContent ────────────────────────────────────────────── */
    hu_json_value_t *sc = hu_json_object_get(json, "serverContent");
    if (sc) {
        /* modelTurn.parts[].inlineData (audio output) */
        hu_json_value_t *mt = hu_json_object_get(sc, "modelTurn");
        if (mt) {
            hu_json_value_t *parts = hu_json_object_get(mt, "parts");
            if (parts && parts->type == HU_JSON_ARRAY && parts->data.array.len > 0) {
                hu_json_value_t *part0 = parts->data.array.items[0];
                hu_json_value_t *id = hu_json_object_get(part0, "inlineData");
                if (id) {
                    const char *data = hu_json_get_string(id, "data");
                    if (data && data[0]) {
                        size_t dlen = strlen(data);
                        out->audio_base64 = hu_strndup(alloc, data, dlen);
                        if (!out->audio_base64) {
                            hu_json_free(alloc, json);
                            alloc->free(alloc->ctx, msg, msg_len + 1);
                            return HU_ERR_OUT_OF_MEMORY;
                        }
                        out->audio_base64_len = dlen;
                        gl_copy_event_type("serverContent.modelTurn.audio", out);
                    }
                }
            }
        }

        /* inputTranscription */
        hu_json_value_t *it = hu_json_object_get(sc, "inputTranscription");
        if (it) {
            const char *text = hu_json_get_string(it, "text");
            if (text && text[0]) {
                size_t tlen = strlen(text);
                out->transcript = hu_strndup(alloc, text, tlen);
                if (!out->transcript) {
                    hu_json_free(alloc, json);
                    alloc->free(alloc->ctx, msg, msg_len + 1);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                out->transcript_len = tlen;
                gl_copy_event_type("serverContent.inputTranscription", out);
            }
        }

        /* outputTranscription */
        hu_json_value_t *ot = hu_json_object_get(sc, "outputTranscription");
        if (ot) {
            const char *text = hu_json_get_string(ot, "text");
            if (text && text[0]) {
                if (!out->transcript) {
                    size_t tlen = strlen(text);
                    out->transcript = hu_strndup(alloc, text, tlen);
                    if (!out->transcript) {
                        hu_json_free(alloc, json);
                        alloc->free(alloc->ctx, msg, msg_len + 1);
                        return HU_ERR_OUT_OF_MEMORY;
                    }
                    out->transcript_len = tlen;
                }
                gl_copy_event_type("serverContent.outputTranscription", out);
            }
        }

        /* interrupted — stop playback, clear queue */
        bool is_interrupted = hu_json_get_bool(sc, "interrupted", false);
        if (is_interrupted) {
            out->interrupted = true;
            gl_copy_event_type("serverContent.interrupted", out);
        }

        /* generationComplete — model done generating, playback may continue */
        bool gen_complete = hu_json_get_bool(sc, "generationComplete", false);
        if (gen_complete) {
            out->generation_complete = true;
            if (out->type[0] == '\0')
                gl_copy_event_type("serverContent.generationComplete", out);
        }

        /* turnComplete */
        bool turn_complete = hu_json_get_bool(sc, "turnComplete", false);
        if (turn_complete) {
            out->done = true;
            if (out->type[0] == '\0')
                gl_copy_event_type("serverContent.turnComplete", out);
        }
    }

    /* ── toolCall ─────────────────────────────────────────────────── */
    hu_json_value_t *tc = hu_json_object_get(json, "toolCall");
    if (tc) {
        gl_copy_event_type("toolCall", out);
        hu_json_value_t *fcs = hu_json_object_get(tc, "functionCalls");
        if (fcs && fcs->type == HU_JSON_ARRAY && fcs->data.array.len > 0) {
            hu_json_value_t *fc0 = fcs->data.array.items[0];
            const char *name = hu_json_get_string(fc0, "name");
            if (name) {
                size_t nlen = strlen(name);
                out->transcript = hu_strndup(alloc, name, nlen);
                if (!out->transcript) {
                    hu_json_free(alloc, json);
                    alloc->free(alloc->ctx, msg, msg_len + 1);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                out->transcript_len = nlen;
            }
            const char *fid = hu_json_get_string(fc0, "id");
            if (fid && fid[0]) {
                size_t flen = strlen(fid);
                out->tool_call_id = hu_strndup(alloc, fid, flen);
                if (!out->tool_call_id) {
                    hu_json_free(alloc, json);
                    alloc->free(alloc->ctx, msg, msg_len + 1);
                    hu_voice_rt_event_free(alloc, out);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                out->tool_call_id_len = flen;
            }
            hu_json_value_t *args = hu_json_object_get(fc0, "args");
            if (args) {
                char *args_str = NULL;
                size_t args_len = 0;
                if (hu_json_stringify(alloc, args, &args_str, &args_len) == HU_OK && args_str) {
                    out->tool_args_json = args_str;
                    out->tool_args_json_len = args_len;
                }
            }
        }
    }

    /* ── toolCallCancellation ─────────────────────────────────────── */
    hu_json_value_t *tcc = hu_json_object_get(json, "toolCallCancellation");
    if (tcc) {
        gl_copy_event_type("toolCallCancellation", out);
        hu_json_value_t *ids = hu_json_object_get(tcc, "ids");
        if (ids && ids->type == HU_JSON_ARRAY && ids->data.array.len > 0) {
            /* Join cancelled IDs as comma-separated string in transcript */
            hu_json_buf_t id_buf = {0};
            hu_error_t id_err = hu_json_buf_init(&id_buf, alloc);
            if (id_err == HU_OK) {
                for (size_t i = 0; i < ids->data.array.len; i++) {
                    if (i > 0)
                        hu_json_buf_append_raw(&id_buf, ",", 1);
                    hu_json_value_t *v = ids->data.array.items[i];
                    if (v && v->type == HU_JSON_STRING)
                        hu_json_buf_append_raw(&id_buf, v->data.string.ptr, v->data.string.len);
                }
                if (id_buf.len > 0) {
                    out->transcript = hu_strndup(alloc, id_buf.ptr, id_buf.len);
                    out->transcript_len = out->transcript ? strlen(out->transcript) : 0;
                }
                hu_json_buf_free(&id_buf);
            }
        }
    }

    /* ── setupComplete ────────────────────────────────────────────── */
    hu_json_value_t *setup = hu_json_object_get(json, "setupComplete");
    if (setup)
        gl_copy_event_type("setupComplete", out);

    /* ── goAway — server will disconnect soon ─────────────────────── */
    hu_json_value_t *ga = hu_json_object_get(json, "goAway");
    if (ga) {
        gl_copy_event_type("goAway", out);
        const char *tl = hu_json_get_string(ga, "timeLeft");
        if (tl) {
            /* timeLeft is a Duration string like "30s" or "120s" */
            int seconds = 0;
            if (sscanf(tl, "%ds", &seconds) == 1 && seconds > 0)
                out->go_away_ms = seconds * 1000;
            else
                out->go_away_ms = 30000;
        } else {
            out->go_away_ms = 30000;
        }
    }

    /* ── sessionResumptionUpdate ──────────────────────────────────── */
    hu_json_value_t *sru = hu_json_object_get(json, "sessionResumptionUpdate");
    if (sru) {
        const char *handle = hu_json_get_string(sru, "newHandle");
        if (handle && handle[0]) {
            size_t hlen = strlen(handle);
            if (session->resumption_handle) {
                session->alloc->free(session->alloc->ctx, session->resumption_handle,
                                     session->resumption_handle_len + 1);
            }
            session->resumption_handle = hu_strndup(session->alloc, handle, hlen);
            session->resumption_handle_len = hlen;
        }
        if (out->type[0] == '\0')
            gl_copy_event_type("sessionResumptionUpdate", out);
    }

    hu_json_free(alloc, json);
    alloc->free(alloc->ctx, msg, msg_len + 1);
    return HU_OK;
#else
    (void)alloc;
    (void)timeout_ms;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

/* ── Tool support ────────────────────────────────────────────────── */

hu_error_t hu_gemini_live_add_tool(hu_gemini_live_session_t *session, const char *name,
                                   const char *description, const char *parameters_json) {
    if (!session || !name)
        return HU_ERR_INVALID_ARGUMENT;
#if HU_IS_TEST
    (void)description;
    (void)parameters_json;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;
    const char *desc = description ? description : "";
    const char *params = (parameters_json && parameters_json[0]) ? parameters_json : "{}";
    hu_json_buf_t buf = {0};
    hu_error_t err = hu_json_buf_init(&buf, session->alloc);
    if (err != HU_OK)
        return err;

    err = hu_json_buf_append_raw(
        &buf, "{\"setup\":{\"tools\":[{\"functionDeclarations\":[{\"name\":", 52);
    if (err == HU_OK)
        err = hu_json_append_string(&buf, name, strlen(name));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, ",\"description\":", 15);
    if (err == HU_OK)
        err = hu_json_append_string(&buf, desc, strlen(desc));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, ",\"parameters\":", 14);
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, params, strlen(params));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, "}]}]}}", 6);
    if (err == HU_OK)
        err = hu_ws_send((hu_ws_client_t *)session->ws_client, buf.ptr, buf.len);

    hu_json_buf_free(&buf);
    return err;
#else
    (void)description;
    (void)parameters_json;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_gemini_live_send_tool_response(hu_gemini_live_session_t *session, const char *name,
                                             const char *call_id, const char *response_json) {
    if (!session || !name || !call_id)
        return HU_ERR_INVALID_ARGUMENT;
#if HU_IS_TEST
    (void)response_json;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;
    const char *resp = (response_json && response_json[0]) ? response_json : "{}";
    hu_json_buf_t buf = {0};
    hu_error_t err = hu_json_buf_init(&buf, session->alloc);
    if (err != HU_OK)
        return err;
    err = hu_json_buf_append_raw(
        &buf, "{\"toolResponse\":{\"functionResponses\":[{\"name\":", 49);
    if (err == HU_OK)
        err = hu_json_append_string(&buf, name, strlen(name));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, ",\"id\":", 6);
    if (err == HU_OK)
        err = hu_json_append_string(&buf, call_id, strlen(call_id));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, ",\"response\":", 12);
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, resp, strlen(resp));
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, "}]}}", 4);
    if (err == HU_OK)
        err = hu_ws_send((hu_ws_client_t *)session->ws_client, buf.ptr, buf.len);
    hu_json_buf_free(&buf);
    return err;
#else
    (void)response_json;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

/* ── Cleanup ─────────────────────────────────────────────────────── */

void hu_gemini_live_session_destroy(hu_gemini_live_session_t *session) {
    if (!session)
        return;
    hu_allocator_t *alloc = session->alloc;
#if defined(HU_HTTP_CURL) && !HU_IS_TEST
    if (session->ws_client) {
        hu_ws_client_free((hu_ws_client_t *)session->ws_client, alloc);
        session->ws_client = NULL;
    }
#endif
    if (session->resumption_handle) {
        alloc->free(alloc->ctx, session->resumption_handle, session->resumption_handle_len + 1);
    }
    alloc->free(alloc->ctx, session, sizeof(hu_gemini_live_session_t));
}

/* ── Voice Provider vtable ───────────────────────────────────────── */

static hu_error_t gl_vp_connect(void *ctx) {
    return hu_gemini_live_connect((hu_gemini_live_session_t *)ctx);
}

static hu_error_t gl_vp_send_audio(void *ctx, const void *pcm16, size_t len) {
    return hu_gemini_live_send_audio((hu_gemini_live_session_t *)ctx, pcm16, len);
}

static hu_error_t gl_vp_recv_event(void *ctx, hu_allocator_t *alloc, hu_voice_rt_event_t *out,
                                   int timeout_ms) {
    return hu_gemini_live_recv_event((hu_gemini_live_session_t *)ctx, alloc, out, timeout_ms);
}

static hu_error_t gl_vp_add_tool(void *ctx, const char *name, const char *description,
                                 const char *parameters_json) {
    return hu_gemini_live_add_tool((hu_gemini_live_session_t *)ctx, name, description,
                                   parameters_json);
}

static hu_error_t gl_vp_cancel_response(void *ctx) {
    (void)ctx;
    return HU_OK;
}

static hu_error_t gl_vp_send_activity_start(void *ctx) {
    return hu_gemini_live_send_activity_start((hu_gemini_live_session_t *)ctx);
}

static hu_error_t gl_vp_send_activity_end(void *ctx) {
    return hu_gemini_live_send_activity_end((hu_gemini_live_session_t *)ctx);
}

static hu_error_t gl_vp_send_audio_stream_end(void *ctx) {
    return hu_gemini_live_send_audio_stream_end((hu_gemini_live_session_t *)ctx);
}

static void gl_vp_disconnect(void *ctx, hu_allocator_t *alloc) {
    (void)alloc;
    hu_gemini_live_session_destroy((hu_gemini_live_session_t *)ctx);
}

static const char *gl_vp_get_name(void *ctx) {
    (void)ctx;
    return "gemini_live";
}

static hu_error_t gl_vp_reconnect(void *ctx) {
    return hu_gemini_live_reconnect((hu_gemini_live_session_t *)ctx);
}

static hu_error_t gl_vp_send_tool_response(void *ctx, const char *name, const char *call_id,
                                           const char *response_json) {
    return hu_gemini_live_send_tool_response((hu_gemini_live_session_t *)ctx, name, call_id,
                                             response_json);
}

static const hu_voice_provider_vtable_t gemini_live_voice_vtable = {
    .connect = gl_vp_connect,
    .send_audio = gl_vp_send_audio,
    .recv_event = gl_vp_recv_event,
    .add_tool = gl_vp_add_tool,
    .cancel_response = gl_vp_cancel_response,
    .disconnect = gl_vp_disconnect,
    .get_name = gl_vp_get_name,
    .send_activity_start = gl_vp_send_activity_start,
    .send_activity_end = gl_vp_send_activity_end,
    .send_audio_stream_end = gl_vp_send_audio_stream_end,
    .reconnect = gl_vp_reconnect,
    .send_tool_response = gl_vp_send_tool_response,
};

hu_error_t hu_voice_provider_gemini_live_create(hu_allocator_t *alloc,
                                                const hu_gemini_live_config_t *config,
                                                hu_voice_provider_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    hu_gemini_live_session_t *session = NULL;
    hu_error_t err = hu_gemini_live_session_create(alloc, config, &session);
    if (err != HU_OK)
        return err;
    out->ctx = session;
    out->vtable = &gemini_live_voice_vtable;
    return HU_OK;
}
