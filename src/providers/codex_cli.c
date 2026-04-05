#include "human/providers/codex_cli.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/process_util.h"
#include "human/core/string.h"
#include "human/provider.h"
#include <stdlib.h>
#include <string.h>

#define HU_CODEX_CLI_NAME       "codex"
#define HU_CODEX_DEFAULT_MODEL  "codex-mini-latest"
#define HU_CODEX_PROMPT_MAX     65536

typedef struct hu_codex_cli_ctx {
    char *model;
    size_t model_len;
} hu_codex_cli_ctx_t;

#if !HU_IS_TEST
static const char *extract_last_user_message(const hu_chat_message_t *msgs, size_t count,
                                             size_t *out_len) {
    for (size_t i = count; i > 0; i--) {
        if (msgs[i - 1].role == HU_ROLE_USER && msgs[i - 1].content &&
            msgs[i - 1].content_len > 0) {
            *out_len = msgs[i - 1].content_len;
            return msgs[i - 1].content;
        }
    }
    return NULL;
}
#endif /* !HU_IS_TEST */

#if defined(HU_GATEWAY_POSIX) && !HU_IS_TEST
static hu_error_t run_codex(hu_allocator_t *alloc, const char *prompt, size_t prompt_len,
                            char **out, size_t *out_len) {
    if (!alloc || !prompt || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (prompt_len >= HU_CODEX_PROMPT_MAX - 1)
        return HU_ERR_INVALID_ARGUMENT;

    char prompt_buf[HU_CODEX_PROMPT_MAX];
    memcpy(prompt_buf, prompt, prompt_len);
    prompt_buf[prompt_len] = '\0';

    const char *argv[] = {HU_CODEX_CLI_NAME, "--quiet", prompt_buf, NULL};

    hu_run_result_t result = {0};
    hu_error_t err = hu_process_run(alloc, argv, NULL, 4 * 1024 * 1024, &result);
    if (err != HU_OK) {
        hu_run_result_free(alloc, &result);
        return err;
    }
    if (!result.success || !result.stdout_buf) {
        hu_run_result_free(alloc, &result);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    size_t len = result.stdout_len;
    while (len > 0 &&
           (result.stdout_buf[len - 1] == ' ' || result.stdout_buf[len - 1] == '\t' ||
            result.stdout_buf[len - 1] == '\r' || result.stdout_buf[len - 1] == '\n'))
        len--;

    *out = hu_strndup(alloc, result.stdout_buf, len);
    hu_run_result_free(alloc, &result);
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_len = len;
    return HU_OK;
}
#endif /* HU_GATEWAY_POSIX && !HU_IS_TEST */

static hu_error_t codex_cli_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                             const char *system_prompt, size_t system_prompt_len,
                                             const char *message, size_t message_len,
                                             const char *model, size_t model_len,
                                             double temperature, char **out, size_t *out_len) {
    (void)ctx;
    (void)temperature;
    (void)model;
    (void)model_len;
    (void)system_prompt;
    (void)system_prompt_len;
    (void)message;
    (void)message_len;

#if HU_IS_TEST
    const char *mock = "Hello from mock Codex CLI";
    size_t n = strlen(mock);
    char *buf = (char *)alloc->alloc(alloc->ctx, n + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(buf, mock, n + 1);
    *out = buf;
    *out_len = n;
    return HU_OK;
#else
#ifdef HU_GATEWAY_POSIX
    {
        char combined[65536];
        size_t combined_len;
        if (system_prompt && system_prompt_len > 0) {
            if (system_prompt_len + message_len + 4 >= sizeof(combined))
                return HU_ERR_INVALID_ARGUMENT;
            memcpy(combined, system_prompt, system_prompt_len);
            combined[system_prompt_len] = '\n';
            combined[system_prompt_len + 1] = '\n';
            memcpy(combined + system_prompt_len + 2, message, message_len);
            combined_len = system_prompt_len + 2 + message_len;
        } else {
            if (message_len >= sizeof(combined))
                return HU_ERR_INVALID_ARGUMENT;
            memcpy(combined, message, message_len);
            combined_len = message_len;
        }
        return run_codex(alloc, combined, combined_len, out, out_len);
    }
#else
    (void)alloc;
    (void)system_prompt;
    (void)system_prompt_len;
    (void)message;
    (void)message_len;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

static hu_error_t codex_cli_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                                 const char *model, size_t model_len, double temperature,
                                 hu_chat_response_t *out) {
    (void)ctx;
    (void)temperature;
    (void)model;
    (void)model_len;
    (void)request;

#if HU_IS_TEST
    memset(out, 0, sizeof(*out));
    const char *content = "Hello from mock Codex CLI";
    size_t len = strlen(content);
    out->content = hu_strndup(alloc, content, len);
    out->content_len = len;
    out->model = hu_strndup(alloc, "codex-cli", 9);
    out->model_len = out->model ? 9 : 0;
    if (!out->model) {
        if (out->content)
            alloc->free(alloc->ctx, (void *)out->content, out->content_len + 1);
        memset(out, 0, sizeof(*out));
        return HU_ERR_OUT_OF_MEMORY;
    }
    return HU_OK;
#else
#ifdef HU_GATEWAY_POSIX
    {
        size_t prompt_len = 0;
        const char *prompt =
            extract_last_user_message(request->messages, request->messages_count, &prompt_len);
        if (!prompt)
            return HU_ERR_INVALID_ARGUMENT;

        char *text = NULL;
        size_t text_len = 0;
        hu_error_t err = run_codex(alloc, prompt, prompt_len, &text, &text_len);
        if (err != HU_OK)
            return err;

        memset(out, 0, sizeof(*out));
        out->content = text;
        out->content_len = text_len;
        out->model = hu_strndup(alloc, "codex-cli", 9);
        out->model_len = out->model ? 9 : 0;
        if (!out->model) {
            alloc->free(alloc->ctx, (void *)out->content, out->content_len + 1);
            memset(out, 0, sizeof(*out));
            return HU_ERR_OUT_OF_MEMORY;
        }
        return HU_OK;
    }
#else
    (void)alloc;
    (void)request;
    (void)out;
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

/* CLI wrapper shells out to `codex` binary; the CLI does not expose tool-calling or
 * streaming via its API, so these capabilities are inherently unsupported. */
static bool codex_cli_supports_native_tools(void *ctx) {
    (void)ctx;
    return false;
}
static const char *codex_cli_get_name(void *ctx) {
    (void)ctx;
    return "codex-cli";
}
static void codex_cli_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_codex_cli_ctx_t *c = (hu_codex_cli_ctx_t *)ctx;
    if (c && c->model)
        alloc->free(alloc->ctx, c->model, c->model_len + 1);
    if (c)
        alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_provider_vtable_t codex_cli_vtable = {
    .chat_with_system = codex_cli_chat_with_system,
    .chat = codex_cli_chat,
    .supports_native_tools = codex_cli_supports_native_tools,
    .get_name = codex_cli_get_name,
    .deinit = codex_cli_deinit,
    .warmup = NULL,
    .chat_with_tools = NULL,
    /* stream_chat NULL: CLI wrapper cannot stream; codex CLI returns complete output only. */
    .supports_streaming = NULL,
    .supports_vision = NULL,
    .supports_vision_for_model = NULL,
    .stream_chat = NULL,
};

hu_error_t hu_codex_cli_create(hu_allocator_t *alloc, const char *api_key, size_t api_key_len,
                               const char *base_url, size_t base_url_len, hu_provider_t *out) {
    (void)api_key;
    (void)api_key_len;
    (void)base_url;
    (void)base_url_len;
    hu_codex_cli_ctx_t *c = (hu_codex_cli_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->model = hu_strndup(alloc, HU_CODEX_DEFAULT_MODEL, sizeof(HU_CODEX_DEFAULT_MODEL) - 1);
    if (!c->model) {
        alloc->free(alloc->ctx, c, sizeof(*c));
        return HU_ERR_OUT_OF_MEMORY;
    }
    c->model_len = sizeof(HU_CODEX_DEFAULT_MODEL) - 1;
    out->ctx = c;
    out->vtable = &codex_cli_vtable;
    return HU_OK;
}
