#include "human/eval/consistency.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

/* ── Word overlap utilities ──────────────────────────────────────── */

static size_t tokenize_words(const char *text, size_t text_len,
                             const char *words[], size_t word_lens[],
                             size_t max_words) {
    size_t count = 0;
    size_t i = 0;
    while (i < text_len && count < max_words) {
        while (i < text_len && !isalpha((unsigned char)text[i]))
            i++;
        if (i >= text_len)
            break;
        size_t start = i;
        while (i < text_len && isalpha((unsigned char)text[i]))
            i++;
        words[count] = text + start;
        word_lens[count] = i - start;
        count++;
    }
    return count;
}

static bool ci_word_eq(const char *a, size_t alen, const char *b, size_t blen) {
    if (alen != blen)
        return false;
    for (size_t i = 0; i < alen; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return false;
    }
    return true;
}

static float word_overlap(const char *text_a, size_t a_len,
                          const char *text_b, size_t b_len) {
    const char *wa[256], *wb[256];
    size_t wla[256], wlb[256];
    size_t na = tokenize_words(text_a, a_len, wa, wla, 256);
    size_t nb = tokenize_words(text_b, b_len, wb, wlb, 256);
    if (na == 0 || nb == 0)
        return 0.0f;
    size_t matches = 0;
    for (size_t i = 0; i < na; i++) {
        for (size_t j = 0; j < nb; j++) {
            if (ci_word_eq(wa[i], wla[i], wb[j], wlb[j])) {
                matches++;
                break;
            }
        }
    }
    return (float)matches / (float)(na > nb ? na : nb);
}

/* ── Prompt alignment ────────────────────────────────────────────── */

hu_error_t hu_consistency_score_prompt_alignment(
    const char *response, size_t response_len,
    const char *const *traits, size_t traits_count,
    const char *const *preferred_vocab, size_t preferred_count,
    const char *const *avoided_vocab, size_t avoided_count,
    float *score) {
    if (!response || !score)
        return HU_ERR_INVALID_ARGUMENT;

    float trait_score = 0.0f;
    if (traits && traits_count > 0) {
        size_t total_words = 0;
        size_t word_matches = 0;
        for (size_t i = 0; i < traits_count; i++) {
            if (!traits[i])
                continue;
            /* Split each trait string into words (>=3 chars) and match individually.
             * This handles both single-word traits ("friendly") and descriptive
             * traits ("warm and empathetic communicator"). */
            const char *t = traits[i];
            size_t tlen = strlen(t);
            size_t wstart = 0;
            for (size_t j = 0; j <= tlen; j++) {
                if (j == tlen || t[j] == ' ' || t[j] == ',' || t[j] == ';') {
                    size_t wlen = j - wstart;
                    if (wlen >= 3) {
                        total_words++;
                        for (size_t p = 0; p + wlen <= response_len; p++) {
                            if (ci_word_eq(response + p, wlen, t + wstart, wlen)) {
                                word_matches++;
                                break;
                            }
                        }
                    }
                    wstart = j + 1;
                }
            }
        }
        trait_score = total_words > 0 ? (float)word_matches / (float)total_words : 0.0f;
    }

    float vocab_score = 0.0f;
    if (preferred_vocab && preferred_count > 0) {
        size_t pref_found = 0;
        for (size_t i = 0; i < preferred_count; i++) {
            if (preferred_vocab[i] && strstr(response, preferred_vocab[i]))
                pref_found++;
        }
        vocab_score = (float)pref_found / (float)preferred_count;
    }

    float avoid_penalty = 0.0f;
    if (avoided_vocab && avoided_count > 0) {
        size_t avoid_found = 0;
        for (size_t i = 0; i < avoided_count; i++) {
            if (avoided_vocab[i] && strstr(response, avoided_vocab[i]))
                avoid_found++;
        }
        avoid_penalty = (float)avoid_found / (float)avoided_count;
    }

    *score = (trait_score * 0.4f + vocab_score * 0.4f + (1.0f - avoid_penalty) * 0.2f);
    if (*score > 1.0f)
        *score = 1.0f;
    if (*score < 0.0f)
        *score = 0.0f;
    return HU_OK;
}

/* ── Line-to-line consistency ────────────────────────────────────── */

hu_error_t hu_consistency_score_line(
    const char *prev_response, size_t prev_len,
    const char *curr_response, size_t curr_len,
    float *score) {
    if (!prev_response || !curr_response || !score)
        return HU_ERR_INVALID_ARGUMENT;

    float overlap = word_overlap(prev_response, prev_len, curr_response, curr_len);

    /* Length ratio: consistent persona produces similar-length responses */
    float len_ratio = (prev_len > curr_len)
        ? (float)curr_len / (float)prev_len
        : (float)prev_len / (float)curr_len;
    if (prev_len == 0 || curr_len == 0)
        len_ratio = 0.0f;

    *score = overlap * 0.6f + len_ratio * 0.4f;
    return HU_OK;
}

/* ── Q&A stability ───────────────────────────────────────────────── */

hu_error_t hu_consistency_score_qa(
    const char *answer_a, size_t a_len,
    const char *answer_b, size_t b_len,
    float *score) {
    if (!answer_a || !answer_b || !score)
        return HU_ERR_INVALID_ARGUMENT;

    *score = word_overlap(answer_a, a_len, answer_b, b_len);
    return HU_OK;
}

/* ── Lexical fidelity ────────────────────────────────────────────── */

hu_error_t hu_consistency_score_lexical(
    const char *response, size_t response_len,
    const char *const *preferred, size_t preferred_count,
    const char *const *avoided, size_t avoided_count,
    float *score) {
    if (!response || !score)
        return HU_ERR_INVALID_ARGUMENT;

    float pref_score = 1.0f;
    if (preferred && preferred_count > 0) {
        size_t found = 0;
        for (size_t i = 0; i < preferred_count; i++) {
            if (preferred[i] && strstr(response, preferred[i]))
                found++;
        }
        pref_score = (float)found / (float)preferred_count;
    }

    float avoid_score = 1.0f;
    if (avoided && avoided_count > 0) {
        size_t found = 0;
        for (size_t i = 0; i < avoided_count; i++) {
            if (avoided[i] && strstr(response, avoided[i]))
                found++;
        }
        avoid_score = 1.0f - ((float)found / (float)avoided_count);
    }

    (void)response_len;
    *score = pref_score * 0.6f + avoid_score * 0.4f;
    return HU_OK;
}

/* ── Composite & drift ───────────────────────────────────────────── */

float hu_consistency_composite(const hu_consistency_metrics_t *m) {
    if (!m)
        return 0.0f;
    return m->prompt_alignment * 0.3f +
           m->line_consistency * 0.25f +
           m->qa_stability * 0.25f +
           m->lexical_fidelity * 0.2f;
}

void hu_drift_detector_init(hu_drift_detector_t *d, float threshold) {
    if (!d)
        return;
    memset(d, 0, sizeof(*d));
    d->drift_threshold = threshold > 0.0f ? threshold : 0.15f;
}

void hu_drift_detector_set_baseline(hu_drift_detector_t *d,
                                    const hu_consistency_metrics_t *metrics) {
    if (!d || !metrics)
        return;
    d->baseline = *metrics;
    d->baseline.composite = hu_consistency_composite(metrics);
    d->baseline_set = true;
    d->sample_count = 0;
}

bool hu_drift_detector_update(hu_drift_detector_t *d,
                              const hu_consistency_metrics_t *metrics) {
    if (!d || !metrics || !d->baseline_set)
        return false;

    d->current = *metrics;
    d->current.composite = hu_consistency_composite(metrics);
    d->sample_count++;

    float delta = fabsf(d->current.composite - d->baseline.composite);
    d->drift_detected = (delta > d->drift_threshold);
    return d->drift_detected;
}
