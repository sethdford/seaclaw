#ifndef HU_CANVAS_RENDER_H
#define HU_CANVAS_RENDER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

typedef enum hu_canvas_format {
    HU_CANVAS_FORMAT_HTML,
    HU_CANVAS_FORMAT_SVG,
    HU_CANVAS_FORMAT_MERMAID,
    HU_CANVAS_FORMAT_MARKDOWN,
    HU_CANVAS_FORMAT_CODE,
    HU_CANVAS_FORMAT_REACT,
    HU_CANVAS_FORMAT_MOCKUP
} hu_canvas_format_t;

/* Render canvas content to an image file.
 * Under HU_IS_TEST: writes a mock 1x1 PNG to out_path.
 * Returns HU_OK on success, HU_ERR_NOT_SUPPORTED for unknown formats. */
hu_error_t hu_canvas_render_to_image(hu_allocator_t *alloc,
                                     const char *content, size_t content_len,
                                     hu_canvas_format_t format,
                                     const char *out_path, size_t out_path_len);

/* Parse format string to enum. Returns HU_CANVAS_FORMAT_HTML for unknown. */
hu_canvas_format_t hu_canvas_format_from_string(const char *s, size_t len);

#endif
