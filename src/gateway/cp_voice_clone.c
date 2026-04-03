/*
 * Gateway handler for voice.clone — uploads base64 audio to Cartesia and returns voice_id.
 */
#include "cp_internal.h"
#include "human/agent.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/gateway/control_protocol.h"
#include "human/multimodal.h"
#include "human/tts/voice_clone.h"
#include <string.h>

#ifdef HU_GATEWAY_POSIX

#define CP_VOICE_CLONE_MAX_B64 (10 * 1024 * 1024)

hu_error_t cp_voice_clone(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                          const hu_control_protocol_t *proto, const hu_json_value_t *root,
                          char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    if (!out || !out_len || !alloc || !app || !app->config)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    hu_json_value_t *params = hu_json_object_get((hu_json_value_t *)root, "params");
    if (!params)
        return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *audio_val = hu_json_object_get(params, "audio");
    if (!audio_val || audio_val->type != HU_JSON_STRING || audio_val->data.string.len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (audio_val->data.string.len > CP_VOICE_CLONE_MAX_B64)
        return HU_ERR_INVALID_ARGUMENT;

    const char *audio_b64 = audio_val->data.string.ptr;
    size_t audio_b64_len = audio_val->data.string.len;

    const char *mime_type = "audio/wav";
    hu_json_value_t *mime_val = hu_json_object_get(params, "mimeType");
    if (mime_val && mime_val->type == HU_JSON_STRING && mime_val->data.string.len > 0)
        mime_type = mime_val->data.string.ptr;

    const char *name = "My Voice";
    size_t name_len = 8;
    hu_json_value_t *name_val = hu_json_object_get(params, "name");
    if (name_val && name_val->type == HU_JSON_STRING && name_val->data.string.len > 0) {
        name = name_val->data.string.ptr;
        name_len = name_val->data.string.len;
    }

    const char *language = "en";
    size_t language_len = 2;
    hu_json_value_t *lang_val = hu_json_object_get(params, "language");
    if (lang_val && lang_val->type == HU_JSON_STRING && lang_val->data.string.len > 0) {
        language = lang_val->data.string.ptr;
        language_len = lang_val->data.string.len;
    }

    const char *persona_name = NULL;
    size_t persona_name_len = 0;
    hu_json_value_t *persona_val = hu_json_object_get(params, "persona");
    if (persona_val && persona_val->type == HU_JSON_STRING && persona_val->data.string.len > 0) {
        persona_name = persona_val->data.string.ptr;
        persona_name_len = persona_val->data.string.len;
    }

    void *decoded = NULL;
    size_t decoded_len = 0;
    hu_error_t err =
        hu_multimodal_decode_base64(alloc, audio_b64, audio_b64_len, &decoded, &decoded_len);
    if (err != HU_OK)
        return err;

    const hu_config_t *cfg = (const hu_config_t *)app->config;
    const char *api_key = hu_config_get_provider_key(cfg, "cartesia");
    if (!api_key || api_key[0] == '\0') {
        alloc->free(alloc->ctx, decoded, decoded_len);
        return HU_ERR_PROVIDER_AUTH;
    }
    size_t api_key_len = strlen(api_key);

    hu_voice_clone_config_t vcfg = {0};
    vcfg.name = name;
    vcfg.name_len = name_len;
    vcfg.language = language;
    vcfg.language_len = language_len;

    hu_voice_clone_result_t clone_res = {0};
    err = hu_voice_clone_from_bytes(alloc, api_key, api_key_len, (const uint8_t *)decoded,
                                    decoded_len, mime_type, &vcfg, &clone_res);
    alloc->free(alloc->ctx, decoded, decoded_len);
    if (err != HU_OK)
        return err;

    size_t vid_len = strnlen(clone_res.voice_id, sizeof(clone_res.voice_id));
    if (persona_name && persona_name_len > 0 && vid_len > 0) {
        hu_error_t perr = hu_persona_set_voice_id(alloc, persona_name, persona_name_len,
                                                  clone_res.voice_id, vid_len);
        if (perr != HU_OK)
            return perr;
#ifdef HU_HAS_PERSONA
        if (app->agent)
            hu_agent_set_persona(app->agent, persona_name, persona_name_len);
#endif
    }

    hu_json_value_t *resp = hu_json_object_new(alloc);
    if (!resp)
        return HU_ERR_OUT_OF_MEMORY;

    const char *out_name = name;
    size_t out_name_len = name_len;
    if (clone_res.name[0] != '\0') {
        out_name = clone_res.name;
        out_name_len = strnlen(clone_res.name, sizeof(clone_res.name));
    }

    const char *out_lang = language;
    size_t out_lang_len = language_len;
    if (clone_res.language[0] != '\0') {
        out_lang = clone_res.language;
        out_lang_len = strnlen(clone_res.language, sizeof(clone_res.language));
    }

    hu_json_object_set(alloc, resp, "voice_id",
                       hu_json_string_new(alloc, clone_res.voice_id, vid_len));
    hu_json_object_set(alloc, resp, "name", hu_json_string_new(alloc, out_name, out_name_len));
    hu_json_object_set(alloc, resp, "language", hu_json_string_new(alloc, out_lang, out_lang_len));

    return cp_respond_json(alloc, resp, out, out_len);
}

#else

hu_error_t cp_voice_clone(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                          const hu_control_protocol_t *proto, const hu_json_value_t *root,
                          char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    *out = NULL;
    *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_GATEWAY_POSIX */
