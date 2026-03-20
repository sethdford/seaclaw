#include "human/voice/local_tts.h"
#include "human/core/json.h"
#include "human/core/process_util.h"
#include "human/core/string.h"
#include "human/platform.h"
#include <stdio.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

hu_error_t hu_local_tts_synthesize(hu_allocator_t *alloc, const hu_local_tts_config_t *config,
                                   const char *text, char **out_path) {
    if (!alloc || !config || !config->endpoint || !config->endpoint[0] || !out_path)
        return HU_ERR_INVALID_ARGUMENT;
    *out_path = NULL;
    if (!text || !text[0])
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    char tmpl[] = "/tmp/hu_lttsXXXXXX.raw";
    int fd = mkstemps(tmpl, 4);
    if (fd < 0)
        return HU_ERR_IO;
    (void)close(fd);
    size_t plen = strlen(tmpl);
    char *copy = hu_strndup(alloc, tmpl, plen);
    if (!copy) {
        (void)unlink(tmpl);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out_path = copy;
    return HU_OK;
#else
    hu_json_buf_t body = {0};
    if (hu_json_buf_init(&body, alloc) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    if (hu_json_buf_append_raw(&body, "{", 1) != HU_OK)
        goto fail_body;
    if (config->model && config->model[0]) {
        if (hu_json_buf_append_raw(&body, "\"model\":\"", 9) != HU_OK)
            goto fail_body;
        if (hu_json_append_string(&body, config->model, strlen(config->model)) != HU_OK)
            goto fail_body;
        if (hu_json_buf_append_raw(&body, "\",", 2) != HU_OK)
            goto fail_body;
    }
    if (config->voice && config->voice[0]) {
        if (hu_json_buf_append_raw(&body, "\"voice\":\"", 9) != HU_OK)
            goto fail_body;
        if (hu_json_append_string(&body, config->voice, strlen(config->voice)) != HU_OK)
            goto fail_body;
        if (hu_json_buf_append_raw(&body, "\",", 2) != HU_OK)
            goto fail_body;
    }
    if (hu_json_buf_append_raw(&body, "\"input\":", 8) != HU_OK)
        goto fail_body;
    if (hu_json_append_string(&body, text, strlen(text)) != HU_OK)
        goto fail_body;
    if (hu_json_buf_append_raw(&body, "}", 1) != HU_OK)
        goto fail_body;

    char *tmp_dir = hu_platform_get_temp_dir(alloc);
    if (!tmp_dir) {
        hu_json_buf_free(&body);
        return HU_ERR_IO;
    }
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    int pid = (int)getpid();
#else
    int pid = 0;
#endif
    char json_path[256];
    int n = snprintf(json_path, sizeof(json_path), "%s/hu_ltts_%d.json", tmp_dir, pid);
    alloc->free(alloc->ctx, tmp_dir, strlen(tmp_dir) + 1);
    if (n < 0 || (size_t)n >= sizeof(json_path)) {
        hu_json_buf_free(&body);
        return HU_ERR_IO;
    }

    FILE *jf = fopen(json_path, "wb");
    if (!jf) {
        hu_json_buf_free(&body);
        return HU_ERR_IO;
    }
    if (fwrite(body.ptr, 1, body.len, jf) != body.len) {
        fclose(jf);
        unlink(json_path);
        hu_json_buf_free(&body);
        return HU_ERR_IO;
    }
    fclose(jf);
    hu_json_buf_free(&body);

    char out_tmpl[] = "/tmp/hu_ltts_outXXXXXX.raw";
    int out_fd = mkstemps(out_tmpl, 4);
    if (out_fd < 0) {
        unlink(json_path);
        return HU_ERR_IO;
    }
    (void)close(out_fd);

    char data_arg[280];
    n = snprintf(data_arg, sizeof(data_arg), "@%s", json_path);
    if (n < 0 || (size_t)n >= sizeof(data_arg)) {
        unlink(json_path);
        unlink(out_tmpl);
        return HU_ERR_IO;
    }

    const char *argv[] = {"curl", "-s", "-X", "POST", "-H", "Content-Type: application/json", "-d",
                          data_arg,       "-o", out_tmpl, config->endpoint, NULL};

    hu_run_result_t run = {0};
    hu_error_t err = hu_process_run(alloc, argv, NULL, 256, &run);
    unlink(json_path);
    if (err != HU_OK) {
        unlink(out_tmpl);
        hu_run_result_free(alloc, &run);
        return err;
    }
    hu_run_result_free(alloc, &run);

    size_t olen = strlen(out_tmpl);
    char *pcopy = hu_strndup(alloc, out_tmpl, olen);
    if (!pcopy) {
        unlink(out_tmpl);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out_path = pcopy;
    return HU_OK;

fail_body:
    hu_json_buf_free(&body);
    return HU_ERR_OUT_OF_MEMORY;
#endif
}
