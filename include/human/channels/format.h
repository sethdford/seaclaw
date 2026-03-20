#ifndef HU_CHANNELS_FORMAT_H
#define HU_CHANNELS_FORMAT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/*
 * Per-channel outbound message formatting.
 *
 * On success, *out is NUL-terminated; *out_len is the byte length excluding the
 * terminator. Caller frees with: alloc->free(alloc->ctx, *out, *out_len + 1).
 */

hu_error_t hu_channel_format_outbound(hu_allocator_t *alloc, const char *channel_name,
                                      size_t channel_name_len, const char *text, size_t text_len,
                                      char **out, size_t *out_len);

hu_error_t hu_channel_strip_markdown(hu_allocator_t *alloc, const char *text, size_t text_len,
                                     char **out, size_t *out_len);

hu_error_t hu_channel_strip_ai_phrases(hu_allocator_t *alloc, const char *text, size_t text_len,
                                       char **out, size_t *out_len);

#endif /* HU_CHANNELS_FORMAT_H */
