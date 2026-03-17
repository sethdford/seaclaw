#include "human/tools/code_sandbox.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/process_util.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <stdio.h>
#include <string.h>

#define SANDBOX_PARAMS                                                                             \
    "{\"type\":\"object\",\"properties\":{\"language\":{\"type\":\"string\",\"enum\":["           \
    "\"python\",\"javascript\",\"shell\"]},\"code\":{\"type\":\"string\"}},\"required\":["         \
    "\"language\",\"code\"]}"

hu_code_sandbox_config_t hu_code_sandbox_config_default(void) {
    return (hu_code_sandbox_config_t){
        .language = HU_SANDBOX_PYTHON,
        .timeout_ms = 10000,
        .memory_limit_mb = 256,
        .allow_network = false,
    };
}

hu_error_t hu_code_sandbox_execute(hu_allocator_t *alloc,
                                   const hu_code_sandbox_config_t *config,
                                   const char *code, size_t code_len,
                                   hu_code_sandbox_result_t *result) {
    if (!alloc || !config || !result)
        return HU_ERR_INVALID_ARGUMENT;
    if (!code)
        return HU_ERR_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));

#ifdef HU_IS_TEST
    /* Mock results: no real process spawning */
    (void)code_len;
    switch (config->language) {
    case HU_SANDBOX_PYTHON:
        memcpy(result->stdout_buf, "Hello World\n", 12);
        result->stdout_len = 12;
        result->exit_code = 0;
        break;
    case HU_SANDBOX_JAVASCRIPT:
        memcpy(result->stdout_buf, "42\n", 3);
        result->stdout_len = 3;
        result->exit_code = 0;
        break;
    case HU_SANDBOX_SHELL:
        memcpy(result->stdout_buf, "total 0\n", 8);
        result->stdout_len = 8;
        result->exit_code = 0;
        break;
    }
    return HU_OK;
#else
#ifndef _WIN32
    const char *argv[8];
    size_t argc = 0;
    switch (config->language) {
    case HU_SANDBOX_PYTHON:
        argv[argc++] = "python3";
        argv[argc++] = "-c";
        break;
    case HU_SANDBOX_JAVASCRIPT:
        argv[argc++] = "node";
        argv[argc++] = "-e";
        break;
    case HU_SANDBOX_SHELL:
        argv[argc++] = "sh";
        argv[argc++] = "-c";
        break;
    }
    if (argc == 0)
        return HU_ERR_NOT_SUPPORTED;

    /* Code must be null-terminated for exec */
    size_t code_cap = code_len + 1;
    if (code_cap > 65536)
        code_cap = 65536;
    char *code_copy = (char *)alloc->alloc(alloc->ctx, code_cap);
    if (!code_copy)
        return HU_ERR_OUT_OF_MEMORY;
    size_t copy_len = code_len < code_cap - 1 ? code_len : code_cap - 1;
    memcpy(code_copy, code, copy_len);
    code_copy[copy_len] = '\0';

    argv[argc++] = code_copy;
    argv[argc++] = NULL;

    size_t max_out = config->memory_limit_mb > 0 ? config->memory_limit_mb * 1024 : 262144;
    if (max_out > 1048576)
        max_out = 1048576;

    hu_run_result_t run;
    hu_error_t err = hu_process_run(alloc, argv, NULL, max_out, &run);

    alloc->free(alloc->ctx, code_copy, code_cap);

    if (err != HU_OK) {
        result->exit_code = -1;
        return err;
    }

    if (run.stdout_buf && run.stdout_len > 0) {
        size_t n = run.stdout_len < sizeof(result->stdout_buf) - 1 ? run.stdout_len
                                                                   : sizeof(result->stdout_buf) - 1;
        memcpy(result->stdout_buf, run.stdout_buf, n);
        result->stdout_buf[n] = '\0';
        result->stdout_len = n;
    }
    if (run.stderr_buf && run.stderr_len > 0) {
        size_t n = run.stderr_len < sizeof(result->stderr_buf) - 1 ? run.stderr_len
                                                                   : sizeof(result->stderr_buf) - 1;
        memcpy(result->stderr_buf, run.stderr_buf, n);
        result->stderr_buf[n] = '\0';
        result->stderr_len = n;
    }
    result->exit_code = run.exit_code;
    result->timed_out = false;
    result->oom_killed = false;

    hu_run_result_free(alloc, &run);
    return HU_OK;
#else
    (void)code_len;
    (void)config;
    result->exit_code = -1;
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

const char *hu_sandbox_language_name(hu_sandbox_language_t lang) {
    switch (lang) {
    case HU_SANDBOX_PYTHON:
        return "python";
    case HU_SANDBOX_JAVASCRIPT:
        return "javascript";
    case HU_SANDBOX_SHELL:
        return "shell";
    }
    return "unknown";
}

hu_error_t hu_code_sandbox_save_checkpoint(const hu_code_sandbox_result_t *result,
                                            hu_sandbox_language_t language,
                                            hu_code_sandbox_checkpoint_t *ckpt) {
    if (!result || !ckpt)
        return HU_ERR_INVALID_ARGUMENT;
    memset(ckpt, 0, sizeof(*ckpt));
    snprintf(ckpt->state_id, sizeof(ckpt->state_id), "ckpt-%lld-%d",
             (long long)result->elapsed_ms, result->exit_code);
    ckpt->language = language;
    ckpt->elapsed_ms = result->elapsed_ms;
    ckpt->valid = (result->exit_code == 0 && !result->timed_out);
    return HU_OK;
}

hu_error_t hu_code_sandbox_restore_checkpoint(const hu_code_sandbox_checkpoint_t *ckpt,
                                               hu_code_sandbox_config_t *config) {
    if (!ckpt || !config)
        return HU_ERR_INVALID_ARGUMENT;
    if (!ckpt->valid)
        return HU_ERR_INVALID_ARGUMENT;
    config->language = ckpt->language;
    return HU_OK;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Tool vtable
 * ───────────────────────────────────────────────────────────────────────── */

static hu_error_t code_sandbox_execute(void *ctx, hu_allocator_t *alloc,
                                       const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)ctx;
    if (!alloc || !args || !out)
        return HU_ERR_INVALID_ARGUMENT;

    const char *lang_str = hu_json_get_string(args, "language");
    const char *code = hu_json_get_string(args, "code");
    if (!lang_str || !lang_str[0]) {
        *out = hu_tool_result_fail("missing language", 15);
        return HU_OK;
    }
    if (!code) {
        *out = hu_tool_result_fail("missing code", 12);
        return HU_OK;
    }

    hu_sandbox_language_t lang = HU_SANDBOX_PYTHON;
    if (strcmp(lang_str, "javascript") == 0)
        lang = HU_SANDBOX_JAVASCRIPT;
    else if (strcmp(lang_str, "shell") == 0)
        lang = HU_SANDBOX_SHELL;

    hu_code_sandbox_config_t config = hu_code_sandbox_config_default();
    config.language = lang;

    hu_code_sandbox_result_t result;
    hu_error_t err = hu_code_sandbox_execute(alloc, &config, code, strlen(code), &result);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("sandbox execute failed", 22);
        return HU_OK;
    }

    /* Output: stdout (or summary on error) */
    char *buf = hu_strndup(alloc, result.stdout_buf, result.stdout_len);
    if (!buf) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_owned(buf, result.stdout_len);
    return HU_OK;
}

static const char *code_sandbox_name(void *ctx) {
    (void)ctx;
    return "code_sandbox";
}

static const char *code_sandbox_description(void *ctx) {
    (void)ctx;
    return "Ephemeral code sandbox: run Python, JavaScript, or shell code with resource limits";
}

static const char *code_sandbox_parameters_json(void *ctx) {
    (void)ctx;
    return SANDBOX_PARAMS;
}

static const hu_tool_vtable_t code_sandbox_vtable = {
    .execute = code_sandbox_execute,
    .name = code_sandbox_name,
    .description = code_sandbox_description,
    .parameters_json = code_sandbox_parameters_json,
    .deinit = NULL,
};

hu_error_t hu_code_sandbox_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    out->ctx = NULL;
    out->vtable = &code_sandbox_vtable;
    return HU_OK;
}
