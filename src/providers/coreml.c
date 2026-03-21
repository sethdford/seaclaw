/*
 * CoreML / MLX local inference — shells out to `python3 -m mlx_lm.generate` on macOS.
 * Gated by HU_ENABLE_COREML (default OFF).
 */
#include "human/providers/coreml.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/process_util.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COREML_PROMPT_MAX 32768

typedef struct {
    char *default_model;
} coreml_ctx_t;

#if !HU_IS_TEST && defined(__APPLE__) && defined(HU_GATEWAY_POSIX)
static hu_error_t coreml_build_prompt(hu_allocator_t *alloc, const hu_chat_request_t *request,
                                      char *buf, size_t cap, size_t *out_len) {
    if (!request || !buf || cap < 2 || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    size_t off = 0;
    for (size_t i = 0; i < request->messages_count; i++) {
        const hu_chat_message_t *m = &request->messages[i];
        if (m->role == HU_ROLE_TOOL)
            continue;
        const char *role_lbl = "User";
        if (m->role == HU_ROLE_SYSTEM)
            role_lbl = "System";
        else if (m->role == HU_ROLE_ASSISTANT)
            role_lbl = "Assistant";
        const char *txt = m->content;
        size_t tlen = m->content_len;
        if (!txt && m->content_parts && m->content_parts_count > 0) {
            for (size_t p = 0; p < m->content_parts_count; p++) {
                if (m->content_parts[p].tag == HU_CONTENT_PART_TEXT) {
                    txt = m->content_parts[p].data.text.ptr;
                    tlen = m->content_parts[p].data.text.len;
                    break;
                }
            }
        }
        if (!txt || tlen == 0)
            continue;
        if (off >= cap)
            return HU_ERR_INVALID_ARGUMENT;
        int n = snprintf(buf + off, cap - off, "%s: %.*s\n\n", role_lbl, (int)tlen, txt);
        if (n <= 0 || (size_t)n >= cap - off)
            return HU_ERR_INVALID_ARGUMENT;
        off += (size_t)n;
    }
    if (off == 0)
        return HU_ERR_INVALID_ARGUMENT;
    buf[off] = '\0';
    *out_len = off;
    (void)alloc;
    return HU_OK;
}

static hu_error_t coreml_resolve_model_nt(coreml_ctx_t *cc, const hu_chat_request_t *request,
                                          const char *model, size_t model_len, char *buf,
                                          size_t cap) {
    if (!cc || !buf || cap < 2)
        return HU_ERR_INVALID_ARGUMENT;
    size_t n = 0;
    const char *src = NULL;
    if (model && model_len > 0) {
        src = model;
        n = model_len;
    } else if (request && request->model && request->model_len > 0) {
        src = request->model;
        n = request->model_len;
    } else if (cc->default_model && cc->default_model[0]) {
        src = cc->default_model;
        n = strlen(cc->default_model);
    }
    if (!src || n == 0 || n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, src, n);
    buf[n] = '\0';
    return HU_OK;
}
#endif /* !HU_IS_TEST && __APPLE__ && HU_GATEWAY_POSIX */

static hu_error_t coreml_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                              const char *model, size_t model_len, double temperature,
                              hu_chat_response_t *out) {
    (void)temperature;
    coreml_ctx_t *cc = (coreml_ctx_t *)ctx;
    if (!cc || !alloc || !request || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

#if HU_IS_TEST
    (void)model;
    (void)model_len;
    (void)request;
    static const char k_mock[] = "CoreML mock response";
    size_t mlen = sizeof(k_mock) - 1u;
    out->content = hu_strndup(alloc, k_mock, mlen);
    if (!out->content)
        return HU_ERR_OUT_OF_MEMORY;
    out->content_len = mlen;
    return HU_OK;
#elif defined(__APPLE__) && defined(HU_GATEWAY_POSIX)
    char model_buf[1024];
    hu_error_t err = coreml_resolve_model_nt(cc, request, model, model_len, model_buf,
                                             sizeof(model_buf));
    if (err != HU_OK)
        return err;
    const char *mdl = model_buf;

    char prompt_buf[COREML_PROMPT_MAX];
    size_t prompt_len = 0;
    err = coreml_build_prompt(alloc, request, prompt_buf, sizeof(prompt_buf), &prompt_len);
    if (err != HU_OK)
        return err;

    char maxtok[16];
    unsigned mt = request->max_tokens ? request->max_tokens : 1024u;
    if (mt > 32768u)
        mt = 32768u;
    (void)snprintf(maxtok, sizeof(maxtok), "%u", mt);

    const char *argv[] = {"python3",           "-m",   "mlx_lm.generate", "--model", mdl,
                          "--prompt",          prompt_buf, "--max-tokens", maxtok, NULL};

    hu_run_result_t result = {0};
    err = hu_process_run(alloc, argv, NULL, 4u * 1024u * 1024u, &result);
    if (err != HU_OK) {
        hu_run_result_free(alloc, &result);
        return err;
    }
    if (!result.success || !result.stdout_buf) {
        hu_run_result_free(alloc, &result);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    size_t len = result.stdout_len;
    while (len > 0 && (result.stdout_buf[len - 1] == ' ' || result.stdout_buf[len - 1] == '\t' ||
                       result.stdout_buf[len - 1] == '\r' || result.stdout_buf[len - 1] == '\n'))
        len--;

    out->content = hu_strndup(alloc, result.stdout_buf, len);
    hu_run_result_free(alloc, &result);
    if (!out->content)
        return HU_ERR_OUT_OF_MEMORY;
    out->content_len = len;
    return HU_OK;
#else
    (void)request;
    (void)model;
    (void)model_len;
    (void)cc;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

static hu_error_t coreml_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                          const char *system_prompt, size_t system_prompt_len,
                                          const char *message, size_t message_len,
                                          const char *model, size_t model_len, double temperature,
                                          char **out, size_t *out_len) {
    if (!message || message_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

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

    hu_chat_request_t req = {.messages = msgs,
                             .messages_count = nmsg,
                             .model = model,
                             .model_len = model_len,
                             .temperature = temperature,
                             .max_tokens = 1024u};

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err = coreml_chat(ctx, alloc, &req, model, model_len, temperature, &resp);
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

static bool coreml_supports_native_tools(void *ctx) {
    (void)ctx;
    return false;
}

static const char *coreml_get_name(void *ctx) {
    (void)ctx;
    return "coreml";
}

static void coreml_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    coreml_ctx_t *cc = (coreml_ctx_t *)ctx;
    if (cc->default_model)
        alloc->free(alloc->ctx, cc->default_model, strlen(cc->default_model) + 1);
    alloc->free(alloc->ctx, cc, sizeof(coreml_ctx_t));
}

static const hu_provider_vtable_t coreml_vtable = {
    .chat_with_system = coreml_chat_with_system,
    .chat = coreml_chat,
    .supports_native_tools = coreml_supports_native_tools,
    .get_name = coreml_get_name,
    .deinit = coreml_deinit,
};

hu_error_t hu_coreml_provider_create(hu_allocator_t *alloc, const hu_coreml_config_t *config,
                                       hu_provider_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    coreml_ctx_t *ctx = alloc->alloc(alloc->ctx, sizeof(coreml_ctx_t));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    memset(ctx, 0, sizeof(*ctx));
    if (config && config->model_path && config->model_path_len > 0) {
        ctx->default_model = hu_strndup(alloc, config->model_path, config->model_path_len);
        if (!ctx->default_model) {
            alloc->free(alloc->ctx, ctx, sizeof(coreml_ctx_t));
            return HU_ERR_OUT_OF_MEMORY;
        }
    }
    out->ctx = ctx;
    out->vtable = &coreml_vtable;
    return HU_OK;
}
