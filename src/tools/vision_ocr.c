#include "human/tools/vision_ocr.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tools/validation.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void hu_ocr_results_free(hu_allocator_t *alloc, hu_ocr_result_t *results, size_t count) {
    if (!results || !alloc)
        return;
    for (size_t i = 0; i < count; i++) {
        if (results[i].text)
            alloc->free(alloc->ctx, results[i].text, strlen(results[i].text) + 1);
    }
    alloc->free(alloc->ctx, results, count * sizeof(hu_ocr_result_t));
}

hu_error_t hu_vision_ocr_find_target(hu_allocator_t *alloc, const hu_ocr_result_t *results,
                                     size_t count, const char *target, size_t target_len,
                                     double *out_x, double *out_y) {
    (void)alloc;
    if (!results || !target || !out_x || !out_y)
        return HU_ERR_INVALID_ARGUMENT;

    double best_confidence = 0.0;
    bool found = false;

    for (size_t i = 0; i < count; i++) {
        if (!results[i].text)
            continue;
        size_t tlen = strlen(results[i].text);
        if (tlen < target_len)
            continue;
        for (size_t j = 0; j <= tlen - target_len; j++) {
            bool match = true;
            for (size_t k = 0; k < target_len; k++) {
                if (tolower((unsigned char)results[i].text[j + k]) !=
                    tolower((unsigned char)target[k])) {
                    match = false;
                    break;
                }
            }
            if (match && results[i].confidence > best_confidence) {
                *out_x = results[i].x + results[i].width / 2.0;
                *out_y = results[i].y + results[i].height / 2.0;
                best_confidence = results[i].confidence;
                found = true;
                break;
            }
        }
    }
    return found ? HU_OK : HU_ERR_NOT_FOUND;
}

#if defined(HU_IS_TEST)
hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    (void)image_path;
    if (!alloc || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out_count = 3;
    *out = (hu_ocr_result_t *)alloc->alloc(alloc->ctx, 3 * sizeof(hu_ocr_result_t));
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    memset(*out, 0, 3 * sizeof(hu_ocr_result_t));

    (*out)[0] = (hu_ocr_result_t){ .text = hu_strndup(alloc, "File", 4),
                                  .x = 10, .y = 5, .width = 30,
                                  .height = 15, .confidence = 0.98 };
    (*out)[1] = (hu_ocr_result_t){ .text = hu_strndup(alloc, "Edit", 4),
                                  .x = 50, .y = 5, .width = 30,
                                  .height = 15, .confidence = 0.97 };
    (*out)[2] = (hu_ocr_result_t){ .text = hu_strndup(alloc, "Save", 4),
                                   .x = 100, .y = 200, .width = 60,
                                   .height = 20, .confidence = 0.95 };
    return HU_OK;
}

#elif defined(__APPLE__) && defined(HU_ENABLE_VISION_OCR)
extern hu_error_t hu_vision_ocr_recognize_apple(hu_allocator_t *alloc, const char *image_path,
                                                hu_ocr_result_t **out, size_t *out_count);

hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    return hu_vision_ocr_recognize_apple(alloc, image_path, out, out_count);
}

#elif defined(__linux__)
hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    (void)alloc; (void)image_path; (void)out; (void)out_count;
    return HU_ERR_NOT_SUPPORTED;
}

#else
hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    (void)alloc; (void)image_path; (void)out; (void)out_count;
    return HU_ERR_NOT_SUPPORTED;
}
#endif

/* ── Tool vtable ─────────────────────────────────────────────────────── */

static hu_error_t vision_ocr_execute(void *ctx, hu_allocator_t *alloc,
                                     const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)ctx;
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)args;
    const char *msg = "{\"results\":[{\"text\":\"mock OCR text\",\"confidence\":0.99}]}";
    *out = hu_tool_result_ok(msg, strlen(msg));
    return HU_OK;
#else
    if (!args || args->type != HU_JSON_OBJECT) {
        *out = hu_tool_result_fail("invalid JSON arguments", 22);
        return HU_OK;
    }
    const char *path = hu_json_get_string(args, "image_path");
    if (!path) {
        *out = hu_tool_result_fail("missing image_path", 18);
        return HU_OK;
    }
    hu_error_t verr = hu_tool_validate_path(path, NULL, 0);
    if (verr != HU_OK) {
        *out = hu_tool_result_fail("path traversal or invalid path", 30);
        return HU_OK;
    }
    /* No workspace binding on this tool: confine absolute paths to /tmp/ (see file_read workspace). */
    if (path[0] == '/' && strncmp(path, "/tmp/", 5) != 0) {
        *out = hu_tool_result_fail("absolute image_path must be under /tmp/", 40);
        return HU_OK;
    }

    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    hu_error_t err = hu_vision_ocr_recognize(alloc, path, &results, &count);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("OCR not available on this platform", 34);
        return HU_OK;
    }

    hu_json_buf_t jb = {0};
    err = hu_json_buf_init(&jb, alloc);
    if (err != HU_OK) {
        hu_ocr_results_free(alloc, results, count);
        return err;
    }
    hu_json_buf_append_raw(&jb, "{\"results\":[", 12);
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            hu_json_buf_append_raw(&jb, ",", 1);
        hu_json_buf_append_raw(&jb, "{\"text\":", 8);
        const char *t = results[i].text ? results[i].text : "";
        hu_json_append_string(&jb, t, strlen(t));
        char numfmt[128];
        int n = snprintf(numfmt, sizeof(numfmt),
                         ",\"confidence\":%.2f,\"x\":%.1f,\"y\":%.1f}",
                         results[i].confidence, results[i].x, results[i].y);
        if (n > 0 && (size_t)n < sizeof(numfmt))
            hu_json_buf_append_raw(&jb, numfmt, (size_t)n);
    }
    hu_json_buf_append_raw(&jb, "]}", 2);
    hu_ocr_results_free(alloc, results, count);

    char *heap_out = hu_strndup(alloc, jb.ptr, jb.len);
    size_t heap_len = jb.len;
    hu_json_buf_free(&jb);
    if (!heap_out)
        return HU_ERR_OUT_OF_MEMORY;
    out->success = true;
    out->output = heap_out;
    out->output_len = heap_len;
    out->output_owned = true;
    return HU_OK;
#endif
}

static const char *vision_ocr_name(void *ctx) { (void)ctx; return "vision_ocr"; }
static const char *vision_ocr_description(void *ctx) {
    (void)ctx;
    return "Perform OCR on an image to extract text regions with positions";
}
static const char *vision_ocr_parameters_json(void *ctx) {
    (void)ctx;
    return "{\"type\":\"object\",\"properties\":{\"image_path\":{\"type\":\"string\","
           "\"description\":\"Path to image file\"}},\"required\":[\"image_path\"]}";
}

hu_error_t hu_vision_ocr_tool_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    (void)alloc;
    static const hu_tool_vtable_t vtable = {
        .execute = vision_ocr_execute,
        .name = vision_ocr_name,
        .description = vision_ocr_description,
        .parameters_json = vision_ocr_parameters_json,
    };
    out->vtable = &vtable;
    out->ctx = NULL;
    return HU_OK;
}
