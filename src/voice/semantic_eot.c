#include "human/voice/semantic_eot.h"
#include <ctype.h>
#include <string.h>

void hu_semantic_eot_config_default(hu_semantic_eot_config_t *cfg) {
    if (!cfg)
        return;
    cfg->min_utterance_chars = 2;
    cfg->pause_threshold_ms = 400;
    cfg->confidence_threshold = 0.6;
}

static bool ends_with_char(const char *text, size_t len, char c) {
    while (len > 0 && isspace((unsigned char)text[len - 1]))
        len--;
    return len > 0 && text[len - 1] == c;
}

static bool ends_with_str(const char *text, size_t len, const char *suffix) {
    size_t slen = strlen(suffix);
    while (len > 0 && isspace((unsigned char)text[len - 1]))
        len--;
    return len >= slen && memcmp(text + len - slen, suffix, slen) == 0;
}

static bool ci_contains(const char *s, size_t len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen > len)
        return false;
    for (size_t i = 0; i + nlen <= len; i++) {
        size_t j = 0;
        while (j < nlen && tolower((unsigned char)s[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == nlen)
            return true;
    }
    return false;
}

static size_t trimmed_len(const char *text, size_t len) {
    while (len > 0 && isspace((unsigned char)text[len - 1]))
        len--;
    return len;
}

hu_error_t hu_semantic_eot_analyze(const hu_semantic_eot_config_t *cfg, const char *text,
                                   size_t text_len, uint32_t silence_ms,
                                   hu_semantic_eot_result_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    out->is_endpoint = false;
    out->confidence = 0.0;
    out->suggested_signal = HU_TURN_SIGNAL_NONE;

    if (!cfg || !text || text_len == 0)
        return HU_OK;

    size_t tlen = trimmed_len(text, text_len);
    if (tlen < cfg->min_utterance_chars)
        return HU_OK;

    double conf = 0.0;

    /* Syntactic completeness: sentence-ending punctuation */
    if (ends_with_char(text, text_len, '.') || ends_with_char(text, text_len, '!'))
        conf += 0.35;
    else if (ends_with_char(text, text_len, '?'))
        conf += 0.45;

    /* Trailing ellipsis = speaker holding */
    if (ends_with_str(text, text_len, "...")) {
        out->confidence = 0.15;
        out->suggested_signal = HU_TURN_SIGNAL_HOLD;
        out->is_endpoint = false;
        return HU_OK;
    }

    /* Lexical yielding phrases */
    if (ci_contains(text, tlen, "what do you think") ||
        ci_contains(text, tlen, "your thoughts") ||
        ci_contains(text, tlen, "can you help") ||
        ci_contains(text, tlen, "do you know"))
        conf += 0.25;

    /* Short complete utterance (under ~40 chars with punctuation) */
    if (tlen < 40 && conf >= 0.3)
        conf += 0.10;

    /* Silence correlation: more silence → higher confidence */
    if (silence_ms >= cfg->pause_threshold_ms)
        conf += 0.20;
    else if (silence_ms >= cfg->pause_threshold_ms / 2)
        conf += 0.10;

    /* Clamp */
    if (conf > 1.0)
        conf = 1.0;

    out->confidence = conf;
    out->is_endpoint = (conf >= cfg->confidence_threshold);

    if (out->is_endpoint) {
        if (ends_with_char(text, text_len, '?'))
            out->suggested_signal = HU_TURN_SIGNAL_YIELD;
        else
            out->suggested_signal = HU_TURN_SIGNAL_YIELD;
    }

    return HU_OK;
}
