#include "human/security/causal_armor.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define HU_CAUSAL_MAX_SEG 32

void hu_causal_armor_config_default(hu_causal_armor_config_t *cfg) {
    if (!cfg)
        return;
    cfg->dominance_threshold = 0.6;
    cfg->user_intent_floor = 0.3;
    cfg->max_segments = 8;
}

static bool word_in_text(const char *text, size_t tlen, const char *w, size_t wlen) {
    if (!text || tlen == 0 || !w || wlen == 0 || wlen > tlen)
        return false;
    for (size_t i = 0; i + wlen <= tlen; i++) {
        if (memcmp(text + i, w, wlen) != 0)
            continue;
        if (i > 0 && isalnum((unsigned char)text[i - 1]))
            continue;
        if (i + wlen < tlen && isalnum((unsigned char)text[i + wlen]))
            continue;
        return true;
    }
    return false;
}

static size_t extract_key_terms(const char *args, size_t args_len, char terms[][32],
                                size_t max_terms) {
    size_t n = 0;
    size_t i = 0;
    while (i < args_len && n < max_terms) {
        while (i < args_len && !isalnum((unsigned char)args[i]))
            i++;
        size_t start = i;
        while (i < args_len && isalnum((unsigned char)args[i]))
            i++;
        size_t wlen = i - start;
        if (wlen > 3 && wlen < sizeof(terms[0])) {
            memcpy(terms[n], args + start, wlen);
            terms[n][wlen] = '\0';
            n++;
        }
    }
    return n;
}

hu_error_t hu_causal_armor_evaluate(const hu_causal_armor_config_t *cfg,
                                    const hu_causal_segment_t *segments, size_t segment_count,
                                    const char *proposed_tool, size_t tool_len,
                                    const char *proposed_args, size_t args_len,
                                    hu_causal_armor_result_t *out) {
    (void)proposed_tool;
    (void)tool_len;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->is_safe = true;
    out->dominant_segment_idx = SIZE_MAX;

    if (!cfg || !segments || segment_count == 0)
        return HU_OK;

    size_t cap = segment_count;
    if (cap > cfg->max_segments)
        cap = cfg->max_segments;

    char terms[32][32];
    size_t term_count = 0;
    if (proposed_args && args_len > 0)
        term_count = extract_key_terms(proposed_args, args_len, terms, 32);
    if (term_count == 0) {
        out->user_intent_attribution = 1.0;
        out->max_untrusted_attribution = 0.0;
        return HU_OK;
    }

    if (cap > HU_CAUSAL_MAX_SEG)
        cap = HU_CAUSAL_MAX_SEG;
    double counts[HU_CAUSAL_MAX_SEG];
    memset(counts, 0, sizeof(counts));

    for (size_t ti = 0; ti < term_count; ti++) {
        size_t wlen = strlen(terms[ti]);
        for (size_t s = 0; s < cap; s++) {
            if (!segments[s].content || segments[s].content_len == 0)
                continue;
            if (word_in_text(segments[s].content, segments[s].content_len, terms[ti], wlen))
                counts[s] += 1.0;
        }
    }

    double total = 0.0;
    for (size_t s = 0; s < cap; s++)
        total += counts[s];

    if (total <= 0.0) {
        double eq = 1.0 / (double)cap;
        for (size_t s = 0; s < cap; s++)
            counts[s] = eq;
        total = 1.0;
    }

    double user_sum = 0.0;
    double max_untr = 0.0;
    size_t dom_u = SIZE_MAX;
    for (size_t s = 0; s < cap; s++) {
        double inf = counts[s] / total;
        if (segments[s].is_trusted)
            user_sum += inf;
        else {
            if (inf > max_untr) {
                max_untr = inf;
                dom_u = s;
            }
        }
    }

    out->user_intent_attribution = user_sum > 1.0 ? 1.0 : user_sum;
    out->max_untrusted_attribution = max_untr;
    out->dominant_segment_idx = dom_u;

    if (max_untr > cfg->dominance_threshold && user_sum < cfg->user_intent_floor) {
        out->is_safe = false;
        int n = snprintf(out->reason, sizeof(out->reason),
                         "untrusted attribution %.2f exceeds threshold with user intent %.2f",
                         max_untr, user_sum);
        out->reason_len = n > 0 ? (size_t)n : 0;
    }

    return HU_OK;
}

hu_error_t hu_causal_armor_check_grounding(const hu_causal_segment_t *segments,
                                           size_t segment_count, const char *tool_name,
                                           size_t name_len, const char *args, size_t args_len,
                                           double *grounding_score) {
    (void)tool_name;
    (void)name_len;
    if (!grounding_score)
        return HU_ERR_INVALID_ARGUMENT;
    *grounding_score = 1.0;
    if (!segments || segment_count == 0 || !args || args_len == 0)
        return HU_OK;

    char terms[24][32];
    size_t term_count = extract_key_terms(args, args_len, terms, 24);
    if (term_count == 0)
        return HU_OK;

    size_t untrusted_only = 0;
    for (size_t ti = 0; ti < term_count; ti++) {
        size_t wlen = strlen(terms[ti]);
        bool in_trusted = false;
        bool in_untrusted = false;
        for (size_t s = 0; s < segment_count; s++) {
            if (!segments[s].content || segments[s].content_len == 0)
                continue;
            if (word_in_text(segments[s].content, segments[s].content_len, terms[ti], wlen)) {
                if (segments[s].is_trusted)
                    in_trusted = true;
                else
                    in_untrusted = true;
            }
        }
        if (in_untrusted && !in_trusted)
            untrusted_only++;
    }

    *grounding_score = 1.0 - (double)untrusted_only / (double)term_count;
    if (*grounding_score < 0.0)
        *grounding_score = 0.0;
    return HU_OK;
}
