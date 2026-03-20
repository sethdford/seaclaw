#ifndef HU_MULTIMODAL_H
#define HU_MULTIMODAL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "human/provider.h"

#define HU_MULTIMODAL_MAX_IMAGE_SIZE (5 * 1024 * 1024)
#define HU_MULTIMODAL_MAX_AUDIO_SIZE (25 * 1024 * 1024)

typedef enum {
    HU_MODALITY_TEXT = 0,
    HU_MODALITY_IMAGE,
    HU_MODALITY_AUDIO,
    HU_MODALITY_VIDEO
} hu_modality_t;

typedef struct hu_multimodal_content {
    hu_modality_t type;
    const char *data;
    size_t data_len;
    char mime_type[64];
    char description[512];
    size_t description_len;
} hu_multimodal_content_t;

typedef struct hu_provider_capabilities {
    bool supports_image;
    bool supports_audio;
    bool supports_video;
    bool supports_streaming;
} hu_provider_capabilities_t;

hu_error_t hu_multimodal_detect_type(const char *mime, size_t mime_len, hu_modality_t *out);
hu_error_t hu_multimodal_needs_fallback(const hu_provider_capabilities_t *caps,
                                         hu_modality_t type, bool *needs_fallback);
const char *hu_modality_name(hu_modality_t type);

typedef struct hu_multimodal_config {
    uint32_t max_images;
    size_t max_image_size_bytes;
    bool allow_remote_fetch;
    const char *const *allowed_dirs;
    size_t allowed_dirs_count;
} hu_multimodal_config_t;

typedef enum hu_image_ref_type {
    HU_IMAGE_REF_LOCAL,
    HU_IMAGE_REF_URL,
    HU_IMAGE_REF_DATA_URI,
} hu_image_ref_type_t;

typedef struct hu_image_ref {
    hu_image_ref_type_t type;
    const char *value;
    size_t value_len;
} hu_image_ref_t;

/* Base64 encode raw bytes */
hu_error_t hu_multimodal_encode_base64(hu_allocator_t *alloc, const void *data, size_t data_len,
                                       char **out_base64, size_t *out_len);

/* Decode standard RFC 4648 base64 (no PEM headers). Rejects inputs larger than
 * HU_MULTIMODAL_MAX_AUDIO_SIZE when decoded. Caller frees *out_bytes with alloc->free. */
hu_error_t hu_multimodal_decode_base64(hu_allocator_t *alloc, const char *b64, size_t b64_len,
                                       void **out_bytes, size_t *out_len);

/* Detect MIME type from first few bytes */
const char *hu_multimodal_detect_mime(const void *header, size_t header_len);

/* Detect audio MIME type from file path extension (e.g. .wav -> "audio/wav") */
const char *hu_multimodal_detect_audio_mime(const char *path, size_t path_len);

/* Encode a local image file to base64 data URI */
hu_error_t hu_multimodal_encode_image(hu_allocator_t *alloc, const char *file_path,
                                      char **out_data_uri, size_t *out_len);

/* Legacy: encode raw image bytes to base64 (uses system allocator). */
hu_error_t hu_multimodal_encode_image_raw(const void *img_data, size_t len, char **out_base64);

/* Parse [IMAGE:...] markers from text */
hu_error_t hu_multimodal_parse_markers(hu_allocator_t *alloc, const char *text, size_t text_len,
                                       hu_image_ref_t **out_refs, size_t *out_ref_count,
                                       char **out_cleaned_text, size_t *out_cleaned_len);

/* Build provider-specific image content JSON */
hu_error_t hu_multimodal_build_openai_image(hu_allocator_t *alloc, const char *data_uri,
                                            size_t data_uri_len, char **out_json,
                                            size_t *out_json_len);

hu_error_t hu_multimodal_build_anthropic_image(hu_allocator_t *alloc, const char *mime_type,
                                               const char *base64_data, size_t base64_len,
                                               char **out_json, size_t *out_json_len);

hu_error_t hu_multimodal_build_gemini_image(hu_allocator_t *alloc, const char *mime_type,
                                            const char *base64_data, size_t base64_len,
                                            char **out_json, size_t *out_json_len);

/* Route a local file path to audio/video handlers by extension (.mp3/.wav/… or .mp4/…). */
hu_error_t hu_multimodal_route_local_media(hu_allocator_t *alloc, const char *file_path,
                                           size_t path_len, hu_provider_t *provider,
                                           const char *model, size_t model_len, char **out_text,
                                           size_t *out_text_len);

#endif /* HU_MULTIMODAL_H */
