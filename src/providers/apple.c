/*
 * Apple Intelligence provider — connects to an apfel server (OpenAI-compatible)
 * running on localhost for free, fully private on-device inference via Apple's
 * FoundationModels framework.
 *
 * The apfel server exposes /v1/chat/completions at http://127.0.0.1:11434/v1.
 * This provider auto-discovers it and maps to the hu_provider_t vtable.
 *
 * Gated by HU_ENABLE_APPLE_INTELLIGENCE (default OFF on non-Apple).
 */
#include "human/providers/apple.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *base_url;
    size_t base_url_len;
} apple_ctx_t;

static hu_error_t apple_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                             const char *model, size_t model_len, double temperature,
                             hu_chat_response_t *out);

static hu_error_t apple_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                         const char *system_prompt, size_t system_prompt_len,
                                         const char *message, size_t message_len,
                                         const char *model, size_t model_len, double temperature,
                                         char **out, size_t *out_len) {
    hu_chat_message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    size_t nmsg = 0;

    if (system_prompt && system_prompt_len > 0) {
        msgs[nmsg].role = HU_ROLE_SYSTEM;
        msgs[nmsg].content = system_prompt;
        msgs[nmsg].content_len = system_prompt_len;
        nmsg++;
    }
    msgs[nmsg].role = HU_ROLE_USER;
    msgs[nmsg].content = message;
    msgs[nmsg].content_len = message_len;
    nmsg++;

    hu_chat_request_t req = {
        .messages = msgs,
        .messages_count = nmsg,
        .model = model,
        .model_len = model_len,
        .temperature = temperature,
        .max_tokens = 0,
        .tools = NULL,
        .tools_count = 0,
        .timeout_secs = 30,
    };

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err = apple_chat(ctx, alloc, &req, model, model_len, temperature, &resp);
    if (err != HU_OK)
        return err;

    if (resp.content && resp.content_len > 0) {
        *out = hu_strndup(alloc, resp.content, resp.content_len);
        if (!*out) {
            hu_chat_response_free(alloc, &resp);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out_len = resp.content_len;
    } else {
        *out = NULL;
        *out_len = 0;
    }
    hu_chat_response_free(alloc, &resp);
    return HU_OK;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static const char *role_str(hu_role_t role) {
    switch (role) {
    case HU_ROLE_SYSTEM:
        return "system";
    case HU_ROLE_ASSISTANT:
        return "assistant";
    case HU_ROLE_TOOL:
        return "tool";
    default:
        return "user";
    }
}

static hu_error_t apple_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                             const char *model, size_t model_len, double temperature,
                             hu_chat_response_t *out) {
    apple_ctx_t *ac = (apple_ctx_t *)ctx;
    if (!ac || !alloc || !request || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

#if HU_IS_TEST
    (void)model;
    (void)model_len;
    (void)temperature;

    if (request->tools && request->tools_count > 0) {
        hu_tool_call_t *tcs = (hu_tool_call_t *)alloc->alloc(alloc->ctx, sizeof(hu_tool_call_t));
        if (tcs) {
            memset(tcs, 0, sizeof(hu_tool_call_t));
            tcs[0].id = hu_strndup(alloc, "call_mock_apple", 15);
            tcs[0].id_len = 15;
            tcs[0].name = hu_strndup(alloc, "shell", 5);
            tcs[0].name_len = 5;
            tcs[0].arguments = hu_strndup(alloc, "{\"command\":\"ls\"}", 16);
            tcs[0].arguments_len = 16;
            out->tool_calls = tcs;
            out->tool_calls_count = 1;
        }
        out->usage.prompt_tokens = 8;
        out->usage.completion_tokens = 6;
        out->usage.total_tokens = 14;
    } else {
        static const char mock[] = "Hello from Apple Intelligence (on-device)";
        size_t len = sizeof(mock) - 1;
        out->content = hu_strndup(alloc, mock, len);
        out->content_len = out->content ? len : 0;
        out->usage.prompt_tokens = 8;
        out->usage.completion_tokens = 7;
        out->usage.total_tokens = 15;
    }
    return HU_OK;
#else
    /* Build OpenAI-compatible JSON request body */
    hu_json_value_t *root = hu_json_object_new(alloc);
    if (!root)
        return HU_ERR_OUT_OF_MEMORY;

    const char *mdl = HU_APPLE_MODEL_NAME;
    size_t mdl_len = sizeof(HU_APPLE_MODEL_NAME) - 1;
    if (model && model_len > 0) {
        mdl = model;
        mdl_len = model_len;
    }

    hu_json_object_set(alloc, root, "model", hu_json_string_new(alloc, mdl, mdl_len));
    hu_json_object_set(alloc, root, "stream", hu_json_bool_new(alloc, false));
    if (temperature > 0.0)
        hu_json_object_set(alloc, root, "temperature", hu_json_number_new(alloc, temperature));

    hu_json_value_t *msgs_arr = hu_json_array_new(alloc);
    if (!msgs_arr) {
        hu_json_free(alloc, root);
        return HU_ERR_OUT_OF_MEMORY;
    }
    hu_json_object_set(alloc, root, "messages", msgs_arr);

    for (size_t i = 0; i < request->messages_count; i++) {
        const hu_chat_message_t *m = &request->messages[i];
        hu_json_value_t *obj = hu_json_object_new(alloc);
        if (!obj) {
            hu_json_free(alloc, root);
            return HU_ERR_OUT_OF_MEMORY;
        }

        const char *rs = role_str(m->role);
        hu_json_object_set(alloc, obj, "role", hu_json_string_new(alloc, rs, strlen(rs)));
        if (m->content && m->content_len > 0)
            hu_json_object_set(alloc, obj, "content",
                               hu_json_string_new(alloc, m->content, m->content_len));

        hu_json_array_push(alloc, msgs_arr, obj);
    }

    /* Tool definitions — apfel supports OpenAI-style function calling */
    if (request->tools && request->tools_count > 0) {
        hu_json_value_t *tools_arr = hu_json_array_new(alloc);
        if (tools_arr) {
            for (size_t i = 0; i < request->tools_count; i++) {
                const hu_tool_spec_t *ts = &request->tools[i];
                hu_json_value_t *tool_obj = hu_json_object_new(alloc);
                hu_json_object_set(alloc, tool_obj, "type",
                                   hu_json_string_new(alloc, "function", 8));
                hu_json_value_t *fn_obj = hu_json_object_new(alloc);
                hu_json_object_set(alloc, fn_obj, "name",
                                   hu_json_string_new(alloc, ts->name, ts->name_len));
                if (ts->description && ts->description_len > 0)
                    hu_json_object_set(
                        alloc, fn_obj, "description",
                        hu_json_string_new(alloc, ts->description, ts->description_len));
                if (ts->parameters_json && ts->parameters_json_len > 0) {
                    hu_json_value_t *params = NULL;
                    if (hu_json_parse(alloc, ts->parameters_json, ts->parameters_json_len,
                                      &params) == HU_OK &&
                        params)
                        hu_json_object_set(alloc, fn_obj, "parameters", params);
                }
                hu_json_object_set(alloc, tool_obj, "function", fn_obj);
                hu_json_array_push(alloc, tools_arr, tool_obj);
            }
            hu_json_object_set(alloc, root, "tools", tools_arr);
        }
    }

    char *body = NULL;
    size_t body_len = 0;
    hu_error_t err = hu_json_stringify(alloc, root, &body, &body_len);
    hu_json_free(alloc, root);
    if (err != HU_OK)
        return err;

    /* Build the /v1/chat/completions URL */
    const char *base = ac->base_url ? ac->base_url : HU_APPLE_DEFAULT_BASE_URL;
    size_t base_len = ac->base_url_len ? ac->base_url_len : (sizeof(HU_APPLE_DEFAULT_BASE_URL) - 1);

    char url_buf[512];
    while (base_len > 0 && base[base_len - 1] == '/')
        base_len--;
    int n = snprintf(url_buf, sizeof(url_buf), "%.*s/chat/completions", (int)base_len, base);
    if (n <= 0 || (size_t)n >= sizeof(url_buf)) {
        alloc->free(alloc->ctx, body, body_len);
        return HU_ERR_INVALID_ARGUMENT;
    }

    hu_http_response_t hresp = {0};
    err = hu_http_post_json(alloc, url_buf, NULL, body, body_len, &hresp);
    alloc->free(alloc->ctx, body, body_len);
    if (err != HU_OK)
        return err;

    if (hresp.status_code < 200 || hresp.status_code >= 300) {
        hu_http_response_free(alloc, &hresp);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    /* Parse OpenAI-compatible JSON response */
    hu_json_value_t *parsed = NULL;
    err = hu_json_parse(alloc, hresp.body, hresp.body_len, &parsed);
    hu_http_response_free(alloc, &hresp);
    if (err != HU_OK)
        return err;

    hu_json_value_t *choices = hu_json_object_get(parsed, "choices");
    if (choices && choices->type == HU_JSON_ARRAY && choices->data.array.len > 0) {
        hu_json_value_t *choice0 = choices->data.array.items[0];
        hu_json_value_t *msg = hu_json_object_get(choice0, "message");
        if (msg && msg->type == HU_JSON_OBJECT) {
            const char *content = hu_json_get_string(msg, "content");
            if (content) {
                size_t clen = strlen(content);
                out->content = hu_strndup(alloc, content, clen);
                out->content_len = out->content ? clen : 0;
            }

            /* Parse tool_calls if present */
            hu_json_value_t *tc_arr = hu_json_object_get(msg, "tool_calls");
            if (tc_arr && tc_arr->type == HU_JSON_ARRAY && tc_arr->data.array.len > 0) {
                size_t tc_count = tc_arr->data.array.len;
                hu_tool_call_t *tcs =
                    (hu_tool_call_t *)alloc->alloc(alloc->ctx, tc_count * sizeof(hu_tool_call_t));
                if (tcs) {
                    memset(tcs, 0, tc_count * sizeof(hu_tool_call_t));
                    size_t valid = 0;
                    for (size_t j = 0; j < tc_count; j++) {
                        hu_json_value_t *tc = tc_arr->data.array.items[j];
                        const char *tc_id = hu_json_get_string(tc, "id");
                        hu_json_value_t *fn = hu_json_object_get(tc, "function");
                        if (!fn || fn->type != HU_JSON_OBJECT)
                            continue;
                        const char *fn_name = hu_json_get_string(fn, "name");
                        const char *fn_args = hu_json_get_string(fn, "arguments");
                        if (!fn_name)
                            continue;
                        tcs[valid].name = hu_strndup(alloc, fn_name, strlen(fn_name));
                        tcs[valid].name_len = tcs[valid].name ? strlen(fn_name) : 0;
                        if (!tcs[valid].name)
                            continue;
                        tcs[valid].id = tc_id ? hu_strndup(alloc, tc_id, strlen(tc_id)) : NULL;
                        tcs[valid].id_len = (tc_id && tcs[valid].id) ? strlen(tc_id) : 0;
                        tcs[valid].arguments =
                            fn_args ? hu_strndup(alloc, fn_args, strlen(fn_args)) : NULL;
                        tcs[valid].arguments_len =
                            (fn_args && tcs[valid].arguments) ? strlen(fn_args) : 0;
                        valid++;
                    }
                    out->tool_calls = tcs;
                    out->tool_calls_count = valid;
                }
            }
        }
    }

    /* Parse usage */
    hu_json_value_t *usage = hu_json_object_get(parsed, "usage");
    if (usage && usage->type == HU_JSON_OBJECT) {
        hu_json_value_t *pt = hu_json_object_get(usage, "prompt_tokens");
        hu_json_value_t *ct = hu_json_object_get(usage, "completion_tokens");
        hu_json_value_t *tt = hu_json_object_get(usage, "total_tokens");
        if (pt && pt->type == HU_JSON_NUMBER)
            out->usage.prompt_tokens = (uint32_t)pt->data.number;
        if (ct && ct->type == HU_JSON_NUMBER)
            out->usage.completion_tokens = (uint32_t)ct->data.number;
        if (tt && tt->type == HU_JSON_NUMBER)
            out->usage.total_tokens = (uint32_t)tt->data.number;
    }

    hu_json_free(alloc, parsed);
    return HU_OK;
#endif
}

static bool apple_supports_native_tools(void *ctx) {
    (void)ctx;
    return true;
}

static const char *apple_get_name(void *ctx) {
    (void)ctx;
    return "apple";
}

static void apple_deinit(void *ctx, hu_allocator_t *alloc) {
    apple_ctx_t *ac = (apple_ctx_t *)ctx;
    if (!ac)
        return;
    if (ac->base_url)
        hu_str_free(alloc, ac->base_url);
    alloc->free(alloc->ctx, ac, sizeof(*ac));
}

static bool apple_supports_streaming(void *ctx) {
    (void)ctx;
    return false;
}

static const hu_provider_vtable_t apple_vtable = {
    .chat_with_system = apple_chat_with_system,
    .chat = apple_chat,
    .supports_native_tools = apple_supports_native_tools,
    .get_name = apple_get_name,
    .deinit = apple_deinit,
    .warmup = NULL,
    .chat_with_tools = NULL,
    .supports_streaming = apple_supports_streaming,
    .supports_vision = NULL,
    .supports_vision_for_model = NULL,
    .stream_chat = NULL,
};

hu_error_t hu_apple_provider_create(hu_allocator_t *alloc, const hu_apple_config_t *config,
                                    hu_provider_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    apple_ctx_t *ac = (apple_ctx_t *)alloc->alloc(alloc->ctx, sizeof(apple_ctx_t));
    if (!ac)
        return HU_ERR_OUT_OF_MEMORY;
    memset(ac, 0, sizeof(*ac));

    const char *url = HU_APPLE_DEFAULT_BASE_URL;
    size_t url_len = sizeof(HU_APPLE_DEFAULT_BASE_URL) - 1;
    if (config && config->base_url && config->base_url_len > 0) {
        url = config->base_url;
        url_len = config->base_url_len;
    }

    ac->base_url = hu_strndup(alloc, url, url_len);
    if (!ac->base_url) {
        alloc->free(alloc->ctx, ac, sizeof(apple_ctx_t));
        return HU_ERR_OUT_OF_MEMORY;
    }
    ac->base_url_len = url_len;

    out->ctx = ac;
    out->vtable = &apple_vtable;
    return HU_OK;
}

bool hu_apple_probe(hu_allocator_t *alloc, const char *base_url, size_t base_url_len) {
#if HU_IS_TEST
    (void)alloc;
    (void)base_url;
    (void)base_url_len;
    return true;
#else
    if (!alloc)
        return false;

    const char *base = base_url;
    size_t blen = base_url_len;
    if (!base || blen == 0) {
        base = HU_APPLE_DEFAULT_BASE_URL;
        blen = sizeof(HU_APPLE_DEFAULT_BASE_URL) - 1;
    }

    while (blen > 0 && base[blen - 1] == '/')
        blen--;

    char url_buf[512];
    int n = snprintf(url_buf, sizeof(url_buf), "%.*s/models", (int)blen, base);
    if (n <= 0 || (size_t)n >= sizeof(url_buf))
        return false;

    hu_http_response_t hresp = {0};
    hu_error_t err = hu_http_get(alloc, url_buf, NULL, &hresp);
    if (err != HU_OK)
        return false;

    bool ok = (hresp.status_code >= 200 && hresp.status_code < 300);
    hu_http_response_free(alloc, &hresp);
    return ok;
#endif
}
