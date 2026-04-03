#include "human/tools/vision_ocr.h"
#include "human/core/string.h"
#include <ctype.h>
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
        /* Case-insensitive substring search */
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
/* Test mock: return predefined OCR results */
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
                                  .x = 10,
                                  .y = 5,
                                  .width = 30,
                                  .height = 15,
                                  .confidence = 0.98 };
    (*out)[1] = (hu_ocr_result_t){ .text = hu_strndup(alloc, "Edit", 4),
                                  .x = 50,
                                  .y = 5,
                                  .width = 30,
                                  .height = 15,
                                  .confidence = 0.97 };
    (*out)[2] = (hu_ocr_result_t){ .text = hu_strndup(alloc, "Save", 4),
                                   .x = 100,
                                   .y = 200,
                                   .width = 60,
                                   .height = 20,
                                   .confidence = 0.95 };

    return HU_OK;
}

#elif defined(__APPLE__) && defined(HU_ENABLE_VISION_OCR)
/* Apple Vision framework OCR — implemented in vision_ocr_apple.m */
extern hu_error_t hu_vision_ocr_recognize_apple(hu_allocator_t *alloc, const char *image_path,
                                                hu_ocr_result_t **out, size_t *out_count);

hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    return hu_vision_ocr_recognize_apple(alloc, image_path, out, out_count);
}

#elif defined(__linux__)
/* Linux: try tesseract CLI fallback */
hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    (void)alloc;
    (void)image_path;
    (void)out;
    (void)out_count;
    return HU_ERR_NOT_SUPPORTED; /* TODO: tesseract CLI integration */
}

#else
hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count) {
    (void)alloc;
    (void)image_path;
    (void)out;
    (void)out_count;
    return HU_ERR_NOT_SUPPORTED;
}
#endif
