#ifndef HU_TOOLS_VISION_OCR_H
#define HU_TOOLS_VISION_OCR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct hu_ocr_result {
    char *text;
    double x;
    double y;
    double width;
    double height;
    double confidence;
} hu_ocr_result_t;

/* Perform OCR on an image file. Returns array of recognized text regions.
 * On non-Apple platforms or when HU_ENABLE_VISION_OCR is not set, returns HU_ERR_NOT_SUPPORTED.
 * Under HU_IS_TEST, returns mock results. */
hu_error_t hu_vision_ocr_recognize(hu_allocator_t *alloc, const char *image_path,
                                   hu_ocr_result_t **out, size_t *out_count);

/* Find OCR result matching target text (case-insensitive substring match).
 * Returns the center coordinates of the best match. */
hu_error_t hu_vision_ocr_find_target(hu_allocator_t *alloc, const hu_ocr_result_t *results,
                                     size_t count, const char *target, size_t target_len,
                                     double *out_x, double *out_y);

void hu_ocr_results_free(hu_allocator_t *alloc, hu_ocr_result_t *results, size_t count);

#endif
