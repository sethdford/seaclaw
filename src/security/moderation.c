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
    if (mod_contains(text, text_len, "suicide") || mod_contains(text, text_len, "self harm") ||
        mod_contains(text, text_len, "self-harm") || mod_contains(text, text_len, "kms") ||
        mod_contains(text, text_len, "unalive") || mod_contains(text, text_len, "end it all") ||
        mod_contains(text, text_len, "don't want to be here anymore") ||
        mod_contains(text, text_len, "what's the point") ||
        mod_contains(text, text_len, "no reason to go on") ||
        mod_contains(text, text_len, "better off without me")) {
        out->self_harm = true;
        out->self_harm_score = 0.85;
        out->flagged = true;
    }
    return HU_OK;
}

hu_error_t hu_moderation_check(hu_allocator_t *alloc, const char *text, size_t text_len, hu_moderation_result_t *out) {
    return hu_moderation_check_local(alloc, text, text_len, out);
}

hu_error_t hu_crisis_response_build(hu_allocator_t *alloc, char **out, size_t *out_len) {
    if (!alloc || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
    static const char msg[] =
        "If you're in crisis, please reach out: "
        "988 Suicide & Crisis Lifeline (call/text 988), "
        "Crisis Text Line (text HOME to 741741)";
    *out = (char *)alloc->alloc(alloc->ctx, sizeof(msg));
    if (!*out) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out, msg, sizeof(msg));
    *out_len = sizeof(msg) - 1;
    return HU_OK;
}
