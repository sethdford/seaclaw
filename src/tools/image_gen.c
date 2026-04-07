/* image_generate — OpenAI DALL-E 3 images API (HTTPS). Mocked when HU_IS_TEST. */

#include "human/tools/image_gen.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/platform.h"
#include "human/tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IG_PROMPT_MAX 4000

static const char *ig_name(void *ctx) {
    (void)ctx;
    return "image_generate";
}

static const char *ig_desc(void *ctx) {
    (void)ctx;
    return "Generate an image from a text description using DALL-E 3. Returns the image URL.";
}

static const char *ig_params(void *ctx) {
    (void)ctx;
    return "{\"type\":\"object\",\"properties\":{\"prompt\":{\"type\":\"string\","
           "\"description\":\"Image description\"},\"size\":{\"type\":\"string\","
           "\"enum\":[\"1024x1024\",\"1792x1024\",\"1024x1792\"],"
           "\"description\":\"Image dimensions (default 1024x1024)\"},\"quality\":{"
           "\"type\":\"string\",\"enum\":[\"standard\",\"hd\"],"
           "\"description\":\"Image quality (default standard)\"}},"
           "\"required\":[\"prompt\"]}";
}

static bool ig_size_ok(const char *s) {
    if (!s)
        return false;
    return strcmp(s, "1024x1024") == 0 || strcmp(s, "1792x1024") == 0 ||
           strcmp(s, "1024x1792") == 0;
}

static bool ig_quality_ok(const char *s) {
    if (!s)
        return false;
    return strcmp(s, "standard") == 0 || strcmp(s, "hd") == 0;
}

static char *ig_dup_slice(hu_allocator_t *alloc, const char *s, size_t len) {
    char *p = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (!p)
        return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

static hu_error_t ig_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                             hu_tool_result_t *out) {
    (void)ctx;
    if (!alloc || !args || !out)
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)ig_size_ok;
    (void)ig_quality_ok;
    const char *prompt = hu_json_get_string(args, "prompt");
    if (!prompt || !prompt[0]) {
        *out = hu_tool_result_fail("prompt is required", 18);
        return HU_OK;
    }
    size_t plen = strlen(prompt);
    if (plen > 50)
        plen = 50;
    char mock[512];
    int n = snprintf(mock, sizeof(mock),
                     "https://oaidalleapiprodscus.blob.core.windows.net/mock/%.*s.png",
                     (int)plen, prompt);
    if (n <= 0 || (size_t)n >= sizeof(mock)) {
        *out = hu_tool_result_fail("mock url overflow", 17);
        return HU_OK;
    }
    char *copy = ig_dup_slice(alloc, mock, (size_t)n);
    if (!copy) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_owned(copy, (size_t)n);
    return HU_OK;
#else
    if (args->type != HU_JSON_OBJECT) {
        *out = hu_tool_result_fail("invalid arguments", 17);
        return HU_OK;
    }

    const char *prompt = hu_json_get_string(args, "prompt");
    if (!prompt || !prompt[0]) {
        *out = hu_tool_result_fail("prompt is required", 18);
        return HU_OK;
    }
    size_t plen_full = strlen(prompt);
    if (plen_full > IG_PROMPT_MAX) {
        *out = hu_tool_result_fail("prompt too long", 15);
        return HU_OK;
    }

    const char *size = hu_json_get_string(args, "size");
    if (!size)
        size = "1024x1024";
    else if (!ig_size_ok(size)) {
        *out = hu_tool_result_fail("invalid size", 12);
        return HU_OK;
    }

    const char *quality = hu_json_get_string(args, "quality");
    if (!quality)
        quality = "standard";
    else if (!ig_quality_ok(quality)) {
        *out = hu_tool_result_fail("invalid quality", 15);
        return HU_OK;
    }

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !api_key[0]) {
        *out = hu_tool_result_fail("OPENAI_API_KEY not set", 22);
        return HU_OK;
    }

    char auth[512];
    int alen = snprintf(auth, sizeof(auth), "Bearer %s", api_key);
    if (alen <= 0 || (size_t)alen >= sizeof(auth)) {
        *out = hu_tool_result_fail("API key too long", 16);
        return HU_OK;
    }

    hu_json_value_t *root = hu_json_object_new(alloc);
    if (!root) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }

    hu_error_t je = HU_OK;
    hu_json_value_t *jv = hu_json_string_new(alloc, "dall-e-3", 8);
    if (!jv) {
        je = HU_ERR_OUT_OF_MEMORY;
        goto json_fail;
    }
    je = hu_json_object_set(alloc, root, "model", jv);
    if (je != HU_OK) {
        hu_json_free(alloc, jv);
        goto json_fail;
    }
    jv = hu_json_string_new(alloc, prompt, plen_full);
    if (!jv) {
        je = HU_ERR_OUT_OF_MEMORY;
        goto json_fail;
    }
    je = hu_json_object_set(alloc, root, "prompt", jv);
    if (je != HU_OK) {
        hu_json_free(alloc, jv);
        goto json_fail;
    }
    jv = hu_json_number_new(alloc, 1.0);
    if (!jv) {
        je = HU_ERR_OUT_OF_MEMORY;
        goto json_fail;
    }
    je = hu_json_object_set(alloc, root, "n", jv);
    if (je != HU_OK) {
        hu_json_free(alloc, jv);
        goto json_fail;
    }
    jv = hu_json_string_new(alloc, size, strlen(size));
    if (!jv) {
        je = HU_ERR_OUT_OF_MEMORY;
        goto json_fail;
    }
    je = hu_json_object_set(alloc, root, "size", jv);
    if (je != HU_OK) {
        hu_json_free(alloc, jv);
        goto json_fail;
    }
    jv = hu_json_string_new(alloc, quality, strlen(quality));
    if (!jv) {
        je = HU_ERR_OUT_OF_MEMORY;
        goto json_fail;
    }
    je = hu_json_object_set(alloc, root, "quality", jv);
    if (je != HU_OK) {
        hu_json_free(alloc, jv);
        goto json_fail;
    }
    jv = hu_json_string_new(alloc, "url", 3);
    if (!jv) {
        je = HU_ERR_OUT_OF_MEMORY;
        goto json_fail;
    }
    je = hu_json_object_set(alloc, root, "response_format", jv);
    if (je != HU_OK) {
        hu_json_free(alloc, jv);
        goto json_fail;
    }

    char *body = NULL;
    size_t body_len = 0;
    je = hu_json_stringify(alloc, root, &body, &body_len);
    hu_json_free(alloc, root);
    if (je != HU_OK || !body) {
        *out = hu_tool_result_fail("failed to build request", 23);
        return je != HU_OK ? je : HU_ERR_OUT_OF_MEMORY;
    }

    hu_http_response_t resp = {0};
    je = hu_http_post_json(alloc, "https://api.openai.com/v1/images/generations", auth, body,
                           body_len, &resp);
    alloc->free(alloc->ctx, body, body_len + 1);
    if (je != HU_OK) {
        hu_http_response_free(alloc, &resp);
        if (je == HU_ERR_NOT_SUPPORTED)
            *out = hu_tool_result_fail("HTTP client unavailable (build with libcurl)", 44);
        else
            *out = hu_tool_result_fail("DALL-E API request failed", 25);
        return HU_OK;
    }
    if (!resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        *out = hu_tool_result_fail("DALL-E API request failed", 25);
        return HU_OK;
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        hu_json_value_t *ej = NULL;
        hu_error_t pe =
            hu_json_parse(alloc, resp.body, resp.body_len, &ej);
        const char *emsg = NULL;
        if (pe == HU_OK && ej) {
            const hu_json_value_t *er = hu_json_object_get(ej, "error");
            if (er && er->type == HU_JSON_OBJECT)
                emsg = hu_json_get_string(er, "message");
        }
        if (emsg && emsg[0]) {
            size_t el = strlen(emsg);
            char *owned = ig_dup_slice(alloc, emsg, el);
            hu_json_free(alloc, ej);
            hu_http_response_free(alloc, &resp);
            if (!owned) {
                *out = hu_tool_result_fail("out of memory", 13);
                return HU_ERR_OUT_OF_MEMORY;
            }
            *out = hu_tool_result_fail_owned(owned, el);
            return HU_OK;
        }
        hu_json_free(alloc, ej);
        hu_http_response_free(alloc, &resp);
        *out = hu_tool_result_fail("DALL-E API error", 16);
        return HU_OK;
    }

    hu_json_value_t *json = NULL;
    je = hu_json_parse(alloc, resp.body, resp.body_len, &json);
    hu_http_response_free(alloc, &resp);
    if (je != HU_OK || !json) {
        hu_json_free(alloc, json);
        *out = hu_tool_result_fail("failed to parse DALL-E response", 31);
        return HU_OK;
    }

    const hu_json_value_t *data = hu_json_object_get(json, "data");
    const char *url = NULL;
    if (data && data->type == HU_JSON_ARRAY && data->data.array.len > 0 &&
        data->data.array.items && data->data.array.items[0]) {
        const hu_json_value_t *first = data->data.array.items[0];
        if (first->type == HU_JSON_OBJECT)
            url = hu_json_get_string(first, "url");
    }

    if (url && url[0]) {
        size_t ulen = strlen(url);
        char *copy = ig_dup_slice(alloc, url, ulen);
        hu_json_free(alloc, json);
        if (!copy) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(copy, ulen);
        return HU_OK;
    }

    const hu_json_value_t *error = hu_json_object_get(json, "error");
    if (error && error->type == HU_JSON_OBJECT) {
        const char *msg = hu_json_get_string(error, "message");
        if (msg && msg[0]) {
            size_t ml = strlen(msg);
            char *owned = ig_dup_slice(alloc, msg, ml);
            hu_json_free(alloc, json);
            if (!owned) {
                *out = hu_tool_result_fail("out of memory", 13);
                return HU_ERR_OUT_OF_MEMORY;
            }
            *out = hu_tool_result_fail_owned(owned, ml);
            return HU_OK;
        }
    }
    hu_json_free(alloc, json);
    *out = hu_tool_result_fail("no URL in DALL-E response", 25);
    return HU_OK;

json_fail:
    hu_json_free(alloc, root);
    *out = hu_tool_result_fail("failed to build request", 23);
    return je != HU_OK ? je : HU_ERR_OUT_OF_MEMORY;
#endif
}

static const hu_tool_vtable_t image_gen_vtable = {
    .execute = ig_execute,
    .name = ig_name,
    .description = ig_desc,
    .parameters_json = ig_params,
    .deinit = NULL,
};

hu_error_t hu_image_gen_create(hu_allocator_t *alloc, hu_tool_t *out) {
    (void)alloc;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    out->vtable = &image_gen_vtable;
    out->ctx = NULL;
    return HU_OK;
}

hu_error_t hu_image_gen_url_into_buffer(hu_allocator_t *alloc, const char *query, size_t query_len,
                                        char *out_url, size_t out_url_cap) {
    if (!alloc || !query || query_len == 0 || !out_url || out_url_cap < 16)
        return HU_ERR_INVALID_ARGUMENT;
    if (query_len > IG_PROMPT_MAX)
        return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *args = hu_json_object_new(alloc);
    if (!args)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *pv = hu_json_string_new(alloc, query, query_len);
    if (!pv) {
        hu_json_free(alloc, args);
        return HU_ERR_OUT_OF_MEMORY;
    }
    hu_error_t se = hu_json_object_set(alloc, args, "prompt", pv);
    if (se != HU_OK) {
        hu_json_free(alloc, pv);
        hu_json_free(alloc, args);
        return se;
    }

    hu_tool_t tool = {0};
    se = hu_image_gen_create(alloc, &tool);
    if (se != HU_OK) {
        hu_json_free(alloc, args);
        return se;
    }

    hu_tool_result_t tr = {0};
    se = tool.vtable->execute(tool.ctx, alloc, args, &tr);
    hu_json_free(alloc, args);
    if (se != HU_OK) {
        hu_tool_result_free(alloc, &tr);
        return se;
    }
    if (!tr.success || !tr.output) {
        hu_tool_result_free(alloc, &tr);
        return HU_ERR_IO;
    }
    size_t olen = strlen(tr.output);
    if (olen >= out_url_cap) {
        hu_tool_result_free(alloc, &tr);
        return HU_ERR_INVALID_ARGUMENT;
    }
    memcpy(out_url, tr.output, olen + 1);
    hu_tool_result_free(alloc, &tr);
    return HU_OK;
}

hu_error_t hu_image_gen_download(hu_allocator_t *alloc, const char *prompt, size_t prompt_len,
                                 char *out_path, size_t out_path_cap) {
    if (!alloc || !prompt || !prompt_len || !out_path || out_path_cap < 32)
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    int n = snprintf(out_path, out_path_cap, "/tmp/hu_test_image_%.*s.png",
                     (int)(prompt_len > 20 ? 20 : prompt_len), prompt);
    return (n > 0 && (size_t)n < out_path_cap) ? HU_OK : HU_ERR_IO;
#else
    char url[2048];
    hu_error_t err = hu_image_gen_url_into_buffer(alloc, prompt, prompt_len, url, sizeof(url));
    if (err != HU_OK)
        return err;

    hu_http_response_t resp = {0};
    err = hu_http_get(alloc, url, NULL, &resp);
    if (err != HU_OK || resp.status_code != 200 || !resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    char *tmpdir = hu_platform_get_temp_dir(alloc);
    if (!tmpdir) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }
    char tpl[512];
    int tn = snprintf(tpl, sizeof(tpl), "%s/hu_img_XXXXXX.png", tmpdir);
    alloc->free(alloc->ctx, tmpdir, strlen(tmpdir) + 1);
    if (tn <= 0 || (size_t)tn >= sizeof(tpl)) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    int fd = mkstemps(tpl, 4);
    if (fd < 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    size_t body_len = resp.body_len;
    ssize_t written = write(fd, resp.body, body_len);
    hu_http_response_free(alloc, &resp);
    close(fd);

    if (written < 0 || (size_t)written != body_len) {
        unlink(tpl);
        return HU_ERR_IO;
    }

    size_t pl = strlen(tpl);
    if (pl >= out_path_cap) {
        unlink(tpl);
        return HU_ERR_INVALID_ARGUMENT;
    }
    memcpy(out_path, tpl, pl + 1);
    return HU_OK;
#endif
}
