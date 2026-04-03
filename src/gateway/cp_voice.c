/*
 * Gateway handler for voice.transcribe — routes to the configured STT provider.
 * Supports: cartesia (file-based), groq (file-based), gemini (inline base64).
 */
#include "cp_internal.h"
#include "human/config.h"
#include "human/multimodal.h"
#include "human/platform.h"
#include "human/voice.h"
#include <stdio.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#ifdef HU_GATEWAY_POSIX

#define CP_VOICE_MAX_AUDIO_LEN (10 * 1024 * 1024) /* 10 MB base64 limit */

#if !HU_IS_TEST
static const char *mime_ext(const char *mime) {
    if (!mime)
        return ".webm";
    if (strstr(mime, "wav"))
        return ".wav";
    if (strstr(mime, "mp3") || strstr(mime, "mpeg"))
        return ".mp3";
    if (strstr(mime, "ogg"))
        return ".ogg";
    if (strstr(mime, "flac"))
        return ".flac";
    if (strstr(mime, "mp4"))
        return ".mp4";
    return ".webm";
}
#endif /* !HU_IS_TEST */

hu_error_t cp_voice_transcribe(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                               const hu_control_protocol_t *proto, const hu_json_value_t *root,
                               char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    *out = NULL;
    *out_len = 0;

    if (!app || !app->config)
        return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *params = hu_json_object_get((hu_json_value_t *)root, "params");
    if (!params)
        return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *audio_val = hu_json_object_get(params, "audio");
    hu_json_value_t *mime_val = hu_json_object_get(params, "mimeType");

    if (!audio_val || audio_val->type != HU_JSON_STRING || audio_val->data.string.len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (audio_val->data.string.len > CP_VOICE_MAX_AUDIO_LEN)
        return HU_ERR_INVALID_ARGUMENT;

    const char *audio_b64 = audio_val->data.string.ptr;
    size_t audio_b64_len = audio_val->data.string.len;

    const char *mime_type = "audio/webm";
    if (mime_val && mime_val->type == HU_JSON_STRING && mime_val->data.string.len > 0)
        mime_type = mime_val->data.string.ptr;

    const hu_config_t *cfg = (const hu_config_t *)app->config;
    const char *stt_provider = cfg->voice.stt_provider;

    hu_voice_config_t voice_cfg = {0};
    (void)hu_voice_config_from_settings(cfg, &voice_cfg);
    voice_cfg.language = NULL;

    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err;

    /* Cartesia and Groq need a file, not inline base64. Gemini takes base64 directly. */
    bool use_file = (stt_provider && (strcmp(stt_provider, "cartesia") == 0 ||
                                      strcmp(stt_provider, "groq") == 0 ||
                                      strcmp(stt_provider, "local") == 0));

    if (use_file) {
#if HU_IS_TEST
        err = hu_voice_stt_file(alloc, &voice_cfg, "/tmp/test.webm", &text, &text_len);
#else
        /* Decode base64 → temp file → hu_voice_stt_file */
        void *decoded = NULL;
        size_t decoded_len = 0;
        err = hu_multimodal_decode_base64(alloc, audio_b64, audio_b64_len, &decoded, &decoded_len);
        if (err != HU_OK)
            return err;

        char *tmp_dir = hu_platform_get_temp_dir(alloc);
        if (!tmp_dir) {
            alloc->free(alloc->ctx, decoded, decoded_len);
            return HU_ERR_IO;
        }
        const char *ext = mime_ext(mime_type);
        char tmp_path[256];
        int n = snprintf(tmp_path, sizeof(tmp_path), "%s/human_stt_gw%s", tmp_dir, ext);
        alloc->free(alloc->ctx, tmp_dir, strlen(tmp_dir) + 1);
        if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
            alloc->free(alloc->ctx, decoded, decoded_len);
            return HU_ERR_IO;
        }

        FILE *f = fopen(tmp_path, "wb");
        if (!f) {
            alloc->free(alloc->ctx, decoded, decoded_len);
            return HU_ERR_IO;
        }
        if (decoded_len > 0 && fwrite(decoded, 1, decoded_len, f) != decoded_len) {
            fclose(f);
            unlink(tmp_path);
            alloc->free(alloc->ctx, decoded, decoded_len);
            return HU_ERR_IO;
        }
        fclose(f);
        alloc->free(alloc->ctx, decoded, decoded_len);

        err = hu_voice_stt_file(alloc, &voice_cfg, tmp_path, &text, &text_len);
        unlink(tmp_path);
#endif
    } else {
        /* Default: Gemini inline base64 transcription */
        const char *api_key = hu_config_get_provider_key(cfg, "gemini");
        if (!api_key || api_key[0] == '\0')
            return HU_ERR_PROVIDER_AUTH;
        voice_cfg.api_key = api_key;
        voice_cfg.api_key_len = strlen(api_key);
        err = hu_voice_stt_gemini(alloc, &voice_cfg, audio_b64, audio_b64_len, mime_type, &text,
                                  &text_len);
    }

    if (err != HU_OK)
        return err;

    hu_json_value_t *resp = hu_json_object_new(alloc);
    if (!resp) {
        alloc->free(alloc->ctx, text, text_len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    hu_json_object_set(alloc, resp, "text", hu_json_string_new(alloc, text, text_len));
    alloc->free(alloc->ctx, text, text_len + 1);

    err = hu_json_stringify(alloc, resp, out, out_len);
    hu_json_free(alloc, resp);
    return err;
}

hu_error_t cp_voice_config(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                           const hu_control_protocol_t *proto, const hu_json_value_t *root,
                           char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    *out = NULL;
    *out_len = 0;
    if (!app || !app->config)
        return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;

#if HU_IS_TEST
    cp_json_set_str(alloc, obj, "local_stt_endpoint",
                    "http://localhost:8000/v1/audio/transcriptions");
    cp_json_set_str(alloc, obj, "local_tts_endpoint", "http://localhost:8880/v1/audio/speech");
    cp_json_set_str(alloc, obj, "stt_provider", "local");
    cp_json_set_str(alloc, obj, "tts_provider", "local");
    cp_json_set_str(alloc, obj, "tts_voice", "af_heart");
    cp_json_set_str(alloc, obj, "tts_model", "kokoro");
    cp_json_set_str(alloc, obj, "stt_model", "whisper-large-v3");
    cp_json_set_str(alloc, obj, "realtime_voice", "alloy");
#else
    const hu_voice_settings_t *v = &app->config->voice;
    cp_json_set_str(alloc, obj, "local_stt_endpoint",
                    v->local_stt_endpoint ? v->local_stt_endpoint : "");
    cp_json_set_str(alloc, obj, "local_tts_endpoint",
                    v->local_tts_endpoint ? v->local_tts_endpoint : "");
    cp_json_set_str(alloc, obj, "stt_provider", v->stt_provider ? v->stt_provider : "");
    cp_json_set_str(alloc, obj, "tts_provider", v->tts_provider ? v->tts_provider : "");
    cp_json_set_str(alloc, obj, "tts_voice", v->tts_voice ? v->tts_voice : "");
    cp_json_set_str(alloc, obj, "tts_model", v->tts_model ? v->tts_model : "");
    cp_json_set_str(alloc, obj, "stt_model", v->stt_model ? v->stt_model : "");
    cp_json_set_str(alloc, obj, "realtime_voice", v->realtime_voice ? v->realtime_voice : "");
#endif
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

#else /* !HU_GATEWAY_POSIX */

hu_error_t cp_voice_transcribe(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                               const hu_control_protocol_t *proto, const hu_json_value_t *root,
                               char **out, size_t *out_len) {
    (void)alloc; (void)app; (void)conn; (void)proto; (void)root;
    *out = NULL; *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_voice_config(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                           const hu_control_protocol_t *proto, const hu_json_value_t *root,
                           char **out, size_t *out_len) {
    (void)alloc; (void)app; (void)conn; (void)proto; (void)root;
    *out = NULL; *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_GATEWAY_POSIX */
