#include "human/security/moderation.h"
#include <string.h>

static bool mod_contains(const char *text, size_t len, const char *word) {
    size_t wlen = strlen(word);
    if (wlen > len) return false;
    for (size_t i = 0; i <= len - wlen; i++) {
        bool match = true;
        for (size_t j = 0; j < wlen; j++) {
            char t = text[i+j]; char w = word[j];
            if (t >= 'A' && t <= 'Z') t += 32;
            if (w >= 'A' && w <= 'Z') w += 32;
            if (t != w) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

hu_error_t hu_moderation_check_local(hu_allocator_t *alloc, const char *text, size_t text_len, hu_moderation_result_t *out) {
    if (!alloc || !text || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (mod_contains(text, text_len, "kill") || mod_contains(text, text_len, "murder") || mod_contains(text, text_len, "violence")) { out->violence = true; out->violence_score = 0.9; out->flagged = true; }
    if (mod_contains(text, text_len, "hate") && mod_contains(text, text_len, "group")) { out->hate = true; out->hate_score = 0.8; out->flagged = true; }
    return HU_OK;
}

hu_error_t hu_moderation_check(hu_allocator_t *alloc, const char *text, size_t text_len, hu_moderation_result_t *out) {
    return hu_moderation_check_local(alloc, text, text_len, out);
}
