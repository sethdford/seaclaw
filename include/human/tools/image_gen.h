#ifndef HU_TOOLS_IMAGE_GEN_H
#define HU_TOOLS_IMAGE_GEN_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"

hu_error_t hu_image_gen_create(hu_allocator_t *alloc, hu_tool_t *out);

/* Same HTTPS path as the image_generate tool (default size/quality); copies URL into @p out_url.
 * HU_OK on success. HU_ERR_IO if generation failed (no key, HTTP error, parse error). */
hu_error_t hu_image_gen_url_into_buffer(hu_allocator_t *alloc, const char *query, size_t query_len,
                                        char *out_url, size_t out_url_cap);

/** Generate an image and download it to a temp file. Caller must unlink() the file.
 *  Writes the temp path into @p out_path (capacity @p out_path_cap).
 *  HU_OK on success. HU_ERR_IO on failure. */
hu_error_t hu_image_gen_download(hu_allocator_t *alloc, const char *prompt, size_t prompt_len,
                                 char *out_path, size_t out_path_cap);

#endif /* HU_TOOLS_IMAGE_GEN_H */
