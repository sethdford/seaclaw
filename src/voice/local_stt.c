#include "human/voice/local_stt.h"
#include "human/core/json.h"
#include "human/core/process_util.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

#define HU_LOCAL_STT_DEFAULT_MODEL "whisper-large-v3"

hu_error_t hu_local_stt_transcribe(hu_allocator_t *alloc, const hu_local_stt_config_t *config,
                                   const char *audio_path, char **out_text, size_t *out_len) {
    if (!alloc || !config || !config->endpoint || !config->endpoint[0] || !audio_path ||
        !audio_path[0] || !out_text || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out_text = NULL;
    *out_len = 0;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)audio_path;
    const char *mock = "Hello world";
    size_t mlen = strlen(mock);
    char *dup = hu_strndup(alloc, mock, mlen);
    if (!dup)
        return HU_ERR_OUT_OF_MEMORY;
    *out_text = dup;
    *out_len = mlen;
    return HU_OK;
#else
    const char *model =
        (config->model && config->model[0]) ? config->model : HU_LOCAL_STT_DEFAULT_MODEL;

    size_t file_cap = 64 + strlen(audio_path);
    char *file_arg = (char *)alloc->alloc(alloc->ctx, file_cap);
    if (!file_arg)
        return HU_ERR_OUT_OF_MEMORY;
    int n = snprintf(file_arg, file_cap, "file=@%s", audio_path);
    if (n < 0 || (size_t)n >= file_cap) {
        alloc->free(alloc->ctx, file_arg, file_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }

    size_t model_cap = 32 + strlen(model);
    char *model_arg = (char *)alloc->alloc(alloc->ctx, model_cap);
    if (!model_arg) {
        alloc->free(alloc->ctx, file_arg, file_cap);
        return HU_ERR_OUT_OF_MEMORY;
    }
    n = snprintf(model_arg, model_cap, "model=%s", model);
    if (n < 0 || (size_t)n >= model_cap) {
        alloc->free(alloc->ctx, file_arg, file_cap);
        alloc->free(alloc->ctx, model_arg, model_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }

    char *lang_arg = NULL;
    size_t lang_cap = 0;
    if (config->language && config->language[0]) {
        lang_cap = 32 + strlen(config->language);
        lang_arg = (char *)alloc->alloc(alloc->ctx, lang_cap);
        if (!lang_arg) {
            alloc->free(alloc->ctx, file_arg, file_cap);
            alloc->free(alloc->ctx, model_arg, model_cap);
            return HU_ERR_OUT_OF_MEMORY;
        }
        n = snprintf(lang_arg, lang_cap, "language=%s", config->language);
        if (n < 0 || (size_t)n >= lang_cap) {
            alloc->free(alloc->ctx, file_arg, file_cap);
            alloc->free(alloc->ctx, model_arg, model_cap);
            alloc->free(alloc->ctx, lang_arg, lang_cap);
            return HU_ERR_INVALID_ARGUMENT;
        }
    }

    const char *argv[16];
    size_t argc = 0;
    argv[argc++] = "curl";
    argv[argc++] = "-s";
    argv[argc++] = "-X";
    argv[argc++] = "POST";
    argv[argc++] = "-F";
    argv[argc++] = file_arg;
    argv[argc++] = "-F";
    argv[argc++] = model_arg;
    if (lang_arg) {
        argv[argc++] = "-F";
        argv[argc++] = lang_arg;
    }
    argv[argc++] = config->endpoint;
    argv[argc] = NULL;

    if (argc >= 16) {
        alloc->free(alloc->ctx, file_arg, file_cap);
        alloc->free(alloc->ctx, model_arg, model_cap);
        if (lang_arg)
            alloc->free(alloc->ctx, lang_arg, lang_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }

    hu_run_result_t result = {0};
    hu_error_t err = hu_process_run(alloc, argv, NULL, 4 * 1024 * 1024, &result);
    alloc->free(alloc->ctx, file_arg, file_cap);
    alloc->free(alloc->ctx, model_arg, model_cap);
    if (lang_arg)
        alloc->free(alloc->ctx, lang_arg, lang_cap);

    if (err != HU_OK) {
        hu_run_result_free(alloc, &result);
        return err;
    }
    if (!result.success || !result.stdout_buf || result.stdout_len == 0) {
        hu_run_result_free(alloc, &result);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    hu_json_value_t *parsed = NULL;
    err = hu_json_parse(alloc, result.stdout_buf, result.stdout_len, &parsed);
    hu_run_result_free(alloc, &result);
    if (err != HU_OK)
        return HU_ERR_PARSE;

    const char *txt = hu_json_get_string(parsed, "text");
    if (!txt || !txt[0]) {
        hu_json_free(alloc, parsed);
        return HU_ERR_PARSE;
    }
    size_t tlen = strlen(txt);
    char *out = hu_strndup(alloc, txt, tlen);
    hu_json_free(alloc, parsed);
    if (!out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_text = out;
    *out_len = tlen;
    return HU_OK;
#endif
}
