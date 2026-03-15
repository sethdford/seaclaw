#include "human/multimodal.h"
#include <string.h>

static bool match_prefix(const char *mime, size_t mime_len, const char *prefix, size_t prefix_len) {
    if (mime_len < prefix_len)
        return false;
    for (size_t i = 0; i < prefix_len; i++) {
        char a = (char)(mime[i] >= 'A' && mime[i] <= 'Z' ? mime[i] + 32 : mime[i]);
        char b = (char)(prefix[i] >= 'A' && prefix[i] <= 'Z' ? prefix[i] + 32 : prefix[i]);
        if (a != b)
            return false;
    }
    return true;
}

hu_error_t hu_multimodal_detect_type(const char *mime, size_t mime_len, hu_modality_t *out) {
    if (!mime || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (match_prefix(mime, mime_len, "image/", 6)) {
        *out = HU_MODALITY_IMAGE;
        return HU_OK;
    }
    if (match_prefix(mime, mime_len, "audio/", 6)) {
        *out = HU_MODALITY_AUDIO;
        return HU_OK;
    }
    if (match_prefix(mime, mime_len, "video/", 6)) {
        *out = HU_MODALITY_VIDEO;
        return HU_OK;
    }
    *out = HU_MODALITY_TEXT;
    return HU_OK;
}

hu_error_t hu_multimodal_needs_fallback(const hu_provider_capabilities_t *caps,
                                         hu_modality_t type, bool *needs_fallback) {
    if (!caps || !needs_fallback)
        return HU_ERR_INVALID_ARGUMENT;
    switch (type) {
    case HU_MODALITY_TEXT:
        *needs_fallback = false;
        break;
    case HU_MODALITY_IMAGE:
        *needs_fallback = !caps->supports_image;
        break;
    case HU_MODALITY_AUDIO:
        *needs_fallback = !caps->supports_audio;
        break;
    case HU_MODALITY_VIDEO:
        *needs_fallback = !caps->supports_video;
        break;
    default:
        *needs_fallback = true;
        break;
    }
    return HU_OK;
}

const char *hu_modality_name(hu_modality_t type) {
    switch (type) {
    case HU_MODALITY_TEXT:
        return "text";
    case HU_MODALITY_IMAGE:
        return "image";
    case HU_MODALITY_AUDIO:
        return "audio";
    case HU_MODALITY_VIDEO:
        return "video";
    default:
        return "text";
    }
}
