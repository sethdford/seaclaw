#include "human/security/moderation.h"
#include "human/security/normalize.h"
#include "human/core/log.h"
#include <ctype.h>
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

/* True when "kill" at norm[i..] is the k-i-l-l inside "skill", not a standalone token. */
static bool mod_kill_match_is_skill_false_positive(const char *norm, size_t len, size_t i) {
    if (i == 0 || i + 3 >= len)
        return false;
    if (norm[i - 1] != 's')
        return false;
    if (strncmp(norm + i, "kill", 4) != 0)
        return false;
    if (i + 4 < len) {
        if (norm[i + 4] == 'l')
            return (i + 5 == len || !isalnum((unsigned char)norm[i + 5]));
        return false;
    }
    return true;
}

static bool mod_norm_has_kill_not_skill(const char *norm, size_t len) {
    if (len < 4)
        return false;
    for (size_t i = 0; i + 4 <= len; i++) {
        if (strncmp(norm + i, "kill", 4) != 0)
            continue;
        if (mod_kill_match_is_skill_false_positive(norm, len, i))
            continue;
        return true;
    }
    return false;
}

static bool mod_norm_has_violence_not_nonprefix(const char *norm, size_t len) {
    static const char w[] = "violence";
    const size_t wlen = sizeof(w) - 1;
    if (len < wlen)
        return false;
    for (size_t i = 0; i + wlen <= len; i++) {
        if (strncmp(norm + i, w, wlen) != 0)
            continue;
        if (i >= 3 && strncmp(norm + i - 3, "non", 3) == 0)
            continue;
        return true;
    }
    return false;
}

static bool mod_contains_word(const char *text, size_t len, const char *word) {
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
        if (match) {
            bool left_ok = (i == 0) || !isalnum((unsigned char)text[i - 1]);
            bool right_ok = (i + wlen >= len) || !isalnum((unsigned char)text[i + wlen]);
            if (left_ok && right_ok) return true;
        }
    }
    return false;
}

hu_error_t hu_moderation_check_local(hu_allocator_t *alloc, const char *text, size_t text_len, hu_moderation_result_t *out) {
    if (!alloc || !text || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    char norm[4096];
    size_t norm_len = 0;
    hu_error_t nerr = hu_normalize_confusables(text, text_len, norm, sizeof(norm), &norm_len);
    if (nerr != HU_OK)
        return nerr;

    if (norm_len >= sizeof(norm) - 1) {
        hu_log_warn("moderation", NULL,
                    "normalization truncated (%zu bytes input, %zu norm cap) — "
                    "tail content checked by raw-text only",
                    text_len, sizeof(norm));
    }

    if (mod_contains_word(text, text_len, "kill") || mod_contains_word(norm, norm_len, "kill") || mod_norm_has_kill_not_skill(norm, norm_len) ||
        mod_contains_word(text, text_len, "murder") || mod_contains_word(norm, norm_len, "murder") || mod_contains(norm, norm_len, "murder") ||
        mod_contains_word(text, text_len, "violence") || mod_contains_word(norm, norm_len, "violence") || mod_norm_has_violence_not_nonprefix(norm, norm_len)) { out->violence = true; out->violence_score = 0.9; out->flagged = true; }
    if ((mod_contains_word(text, text_len, "hate") && mod_contains_word(text, text_len, "group")) ||
        (mod_contains(norm, norm_len, "hate") && mod_contains(norm, norm_len, "group"))) { out->hate = true; out->hate_score = 0.8; out->flagged = true; }
    /* Self-harm detection with graduated severity.
     * High-specificity phrases get full score; ambiguous frustration phrases
     * get a lower score that signals "empathy needed" not "crisis mode". */
    {
        int high_sev = 0, low_sev = 0;
        if (mod_contains_word(text, text_len, "suicide") || mod_contains(norm, norm_len, "suicide")) high_sev++;
        if (mod_contains(text, text_len, "self harm") || mod_contains(text, text_len, "self-harm") ||
            mod_contains(norm, norm_len, "selfharm") || mod_contains(norm, norm_len, "self-harm")) high_sev++;
        if (mod_contains_word(text, text_len, "kms") || mod_contains(norm, norm_len, "kms")) high_sev++;
        if (mod_contains_word(text, text_len, "unalive") || mod_contains(norm, norm_len, "unalive")) high_sev++;
        if (mod_contains(text, text_len, "end it all") || mod_contains(norm, norm_len, "enditall")) high_sev++;
        if (mod_contains(text, text_len, "better off without me") || mod_contains(norm, norm_len, "betteroffwithoutme")) high_sev++;
        /* Moderately concerning phrases — closer to crisis than casual venting */
        if (mod_contains(text, text_len, "don't want to be here anymore") ||
            mod_contains(norm, norm_len, "don'twanttobehereanymore") ||
            mod_contains(norm, norm_len, "dontwanttobehereanymore")) high_sev++;
        if (mod_contains(text, text_len, "no reason to go on") || mod_contains(norm, norm_len, "noreasontogoon")) high_sev++;
        /* Ambiguous phrases — often venting, not imminent risk */
        if (mod_contains(text, text_len, "what's the point") || mod_contains(norm, norm_len, "what'sthepoint") ||
            mod_contains(norm, norm_len, "whatsthepoint")) low_sev++;
        if (mod_contains(text, text_len, "i can't do this anymore") || mod_contains(norm, norm_len, "ican'tdothisanymore") ||
            mod_contains(norm, norm_len, "icantdothisanymore")) low_sev++;

        if (high_sev > 0) {
            out->self_harm = true;
            out->self_harm_score = 0.85;
            out->flagged = true;
        } else if (low_sev > 0) {
            out->self_harm = true;
            out->self_harm_score = 0.35 + (double)low_sev * 0.1;
            /* flagged only at high score — low-severity signals empathy, not crisis */
            out->flagged = (out->self_harm_score >= 0.6);
        }
    }
    if (mod_contains(text, text_len, "explicit sexual") || mod_contains(norm, norm_len, "explicitsexual") ||
        mod_contains_word(text, text_len, "nsfw") ||
        mod_contains(norm, norm_len, "nsfw") ||
        mod_contains(text, text_len, "pornograph")) {
        out->sexual = true;
        out->sexual_score = 0.7;
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
