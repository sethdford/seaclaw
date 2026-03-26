#include "human/tts/voice_clone.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/security.h"
#include "human/tool.h"
#include "human/tools/validation.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define VOICE_CLONE_PARAMS                                                                  \
    "{\"type\":\"object\",\"properties\":{"                                                 \
    "\"file_path\":{\"type\":\"string\",\"description\":\"Path to source audio file\"},"    \
    "\"name\":{\"type\":\"string\",\"description\":\"Display name for the cloned voice\"}," \
    "\"language\":{\"type\":\"string\",\"description\":\"ISO 639-1 language code\"},"       \
    "\"persona\":{\"type\":\"string\",\"description\":\"Persona to set voice_id on\"}"      \
    "},\"required\":[\"file_path\"]}"

typedef struct {
    const char *workspace_dir;
    size_t workspace_dir_len;
    hu_security_policy_t *policy;
} voice_clone_tool_ctx_t;

static hu_error_t voice_clone_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    voice_clone_tool_ctx_t *c = (voice_clone_tool_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!c || !alloc || !args || args->type != HU_JSON_OBJECT) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_OK;
    }

    const char *file_path = hu_json_get_string(args, "file_path");
    if (!file_path || file_path[0] == '\0') {
        *out = hu_tool_result_fail("file_path required", 18);
        return HU_OK;
    }

    hu_error_t perr = hu_tool_validate_path(file_path, c->workspace_dir,
                                            c->workspace_dir ? c->workspace_dir_len : 0);
    if (perr != HU_OK) {
        *out = hu_tool_result_fail("path traversal or invalid path", 30);
        return HU_OK;
    }

    char resolved[4096];
    const char *open_path = file_path;
    bool is_absolute = (file_path[0] == '/') || (strlen(file_path) >= 2 && file_path[1] == ':' &&
                                                 ((file_path[0] >= 'A' && file_path[0] <= 'Z') ||
                                                  (file_path[0] >= 'a' && file_path[0] <= 'z')));
    if (c->workspace_dir && c->workspace_dir_len > 0 && !is_absolute) {
        size_t n = c->workspace_dir_len;
        if (n >= sizeof(resolved) - 1) {
            *out = hu_tool_result_fail("path too long", 13);
            return HU_OK;
        }
        memcpy(resolved, c->workspace_dir, n);
        if (n > 0 && resolved[n - 1] != '/') {
            resolved[n] = '/';
            n++;
        }
        size_t plen = strlen(file_path);
        if (n + plen >= sizeof(resolved)) {
            *out = hu_tool_result_fail("path too long", 13);
            return HU_OK;
        }
        memcpy(resolved + n, file_path, plen + 1);
        open_path = resolved;
    }

    if (!c->policy || !hu_security_path_allowed(c->policy, open_path, strlen(open_path))) {
        *out = hu_tool_result_fail("path not allowed by policy", 26);
        return HU_OK;
    }

    const char *api_key = getenv("CARTESIA_API_KEY");
    size_t api_key_len = api_key ? strlen(api_key) : 0;
#if HU_IS_TEST
    if (api_key_len == 0) {
        api_key = "test";
        api_key_len = 4;
    }
#else
    if (api_key_len == 0) {
        *out = hu_tool_result_fail("missing CARTESIA_API_KEY", 24);
        return HU_OK;
    }
#endif

    const char *name = hu_json_get_string(args, "name");
    const char *language = hu_json_get_string(args, "language");
    const char *persona = hu_json_get_string(args, "persona");

    hu_voice_clone_config_t cfg;
    hu_voice_clone_config_default(&cfg);
    if (name && name[0]) {
        cfg.name = name;
        cfg.name_len = strlen(name);
    }
    if (language && language[0]) {
        cfg.language = language;
        cfg.language_len = strlen(language);
    }

    hu_voice_clone_result_t vr;
    memset(&vr, 0, sizeof(vr));
    hu_error_t err = hu_voice_clone_from_file(alloc, api_key, api_key_len, open_path, &cfg, &vr);

    if (err == HU_ERR_NOT_SUPPORTED) {
        *out = hu_tool_result_fail("voice cloning not available in this build", 41);
        return HU_OK;
    }
    if (err != HU_OK) {
        if (vr.error[0] != '\0') {
            *out =
                hu_tool_result_fail(vr.error, vr.error_len > 0 ? vr.error_len : strlen(vr.error));
        } else {
            *out = hu_tool_result_fail("voice clone request failed", 26);
        }
        return HU_OK;
    }
    if (!vr.success || vr.voice_id[0] == '\0') {
        if (vr.error[0] != '\0') {
            *out =
                hu_tool_result_fail(vr.error, vr.error_len > 0 ? vr.error_len : strlen(vr.error));
        } else {
            *out = hu_tool_result_fail("voice clone failed", 18);
        }
        return HU_OK;
    }

    size_t vid_len = strlen(vr.voice_id);
    if (persona && persona[0]) {
        size_t persona_len = strlen(persona);
        hu_error_t perr2 =
            hu_persona_set_voice_id(alloc, persona, persona_len, vr.voice_id, vid_len);
        if (perr2 != HU_OK) {
            char *msg = hu_sprintf(alloc, "Cloned voice_id %s but failed to update persona \"%s\"",
                                   vr.voice_id, persona);
            if (msg) {
                *out = hu_tool_result_fail_owned(msg, strlen(msg));
            } else {
                *out = hu_tool_result_fail("cloned voice but persona update failed", 38);
            }
            return HU_OK;
        }
    }

    hu_json_value_t *root = hu_json_object_new(alloc);
    if (!root) {
        *out = hu_tool_result_fail("out of memory", 12);
        return HU_OK;
    }
    hu_json_object_set(alloc, root, "voice_id", hu_json_string_new(alloc, vr.voice_id, vid_len));
    if (persona && persona[0]) {
        size_t plen = strlen(persona);
        hu_json_object_set(alloc, root, "persona_updated",
                           hu_json_string_new(alloc, persona, plen));
    }

    char *json_out = NULL;
    size_t json_out_len = 0;
    err = hu_json_stringify(alloc, root, &json_out, &json_out_len);
    hu_json_free(alloc, root);
    if (err != HU_OK || !json_out) {
        *out = hu_tool_result_fail("failed to serialize result", 26);
        return HU_OK;
    }
    *out = hu_tool_result_ok_owned(json_out, json_out_len);
    return HU_OK;
}

static const char *voice_clone_name(void *ctx) {
    (void)ctx;
    return "voice_clone";
}

static const char *voice_clone_description(void *ctx) {
    (void)ctx;
    return "Clone a voice from an audio file for use with Cartesia TTS";
}

static const char *voice_clone_parameters_json(void *ctx) {
    (void)ctx;
    return VOICE_CLONE_PARAMS;
}

static void voice_clone_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    voice_clone_tool_ctx_t *c = (voice_clone_tool_ctx_t *)ctx;
    if (c->workspace_dir)
        alloc->free(alloc->ctx, (void *)c->workspace_dir, c->workspace_dir_len + 1);
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_tool_vtable_t voice_clone_vtable = {
    .execute = voice_clone_execute,
    .name = voice_clone_name,
    .description = voice_clone_description,
    .parameters_json = voice_clone_parameters_json,
    .deinit = voice_clone_deinit,
};

hu_error_t hu_voice_clone_tool_create(hu_allocator_t *alloc, const char *workspace_dir,
                                      size_t workspace_dir_len, hu_security_policy_t *policy,
                                      hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    voice_clone_tool_ctx_t *c = (voice_clone_tool_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    if (workspace_dir && workspace_dir_len > 0) {
        c->workspace_dir = hu_strndup(alloc, workspace_dir, workspace_dir_len);
        if (!c->workspace_dir) {
            alloc->free(alloc->ctx, c, sizeof(*c));
            return HU_ERR_OUT_OF_MEMORY;
        }
        c->workspace_dir_len = workspace_dir_len;
    }
    c->policy = policy;
    out->ctx = c;
    out->vtable = &voice_clone_vtable;
    return HU_OK;
}
