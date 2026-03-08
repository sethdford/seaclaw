#ifndef SC_CONTEXT_VISION_H
#define SC_CONTEXT_VISION_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/provider.h"
#include <stddef.h>

/* Read an image file and encode it as base64 for the provider.
 * Supports JPEG, PNG, GIF, WEBP.
 * Returns the base64-encoded data and the media type string.
 * Caller owns both returned strings. */
sc_error_t sc_vision_read_image(sc_allocator_t *alloc, const char *path, size_t path_len,
                                char **base64_out, size_t *base64_len,
                                char **media_type_out, size_t *media_type_len);

/* Build a vision request to describe an image.
 * Creates a chat message with the image as a content part and a text prompt
 * asking for a brief description.
 * Caller owns the returned description string. */
sc_error_t sc_vision_describe_image(sc_allocator_t *alloc, sc_provider_t *provider,
                                    const char *image_path, size_t image_path_len,
                                    const char *model, size_t model_len,
                                    char **description_out, size_t *description_len);

/* Build context for the prompt when an image description is available.
 * Returns a context string like:
 * "The user shared an image: [description]. Respond naturally to what you see."
 * Caller owns returned string. Returns NULL if description is NULL. */
char *sc_vision_build_context(sc_allocator_t *alloc, const char *description,
                              size_t description_len, size_t *out_len);

#endif /* SC_CONTEXT_VISION_H */
