#include "human/security/sycophancy_guard.h"
#include <ctype.h>
#include <string.h>

/* ── Pattern databases per sycophancy factor ─────────────────────── */

static const char *agreement_patterns[] = {
    "you're absolutely right",    "i completely agree",
    "that's a great point",       "you make an excellent point",
    "i couldn't agree more",      "exactly right",
    "you're so right",            "that's exactly how i see it",
    "absolutely correct",         "you nailed it",
};
static const size_t agreement_count = sizeof(agreement_patterns) / sizeof(agreement_patterns[0]);

static const char *obsequious_patterns[] = {
    "what a wonderful",           "that's such a great",
    "i'm so impressed",           "what an amazing",
    "you're so talented",         "brilliant insight",
    "incredibly thoughtful",      "what a fantastic",
    "i admire your",              "what a beautiful",
};
static const size_t obsequious_count = sizeof(obsequious_patterns) / sizeof(obsequious_patterns[0]);

static const char *excitement_patterns[] = {
    "that's so exciting",         "i love that",
    "how wonderful",              "that's incredible",
    "i'm thrilled",               "how exciting",
    "that sounds amazing",        "what a great idea",
    "i'm so happy for you",       "that's fantastic news",
};
static const size_t excitement_count = sizeof(excitement_patterns) / sizeof(excitement_patterns[0]);

static size_t ci_count_patterns(const char *text, size_t text_len,
                                const char *const *patterns, size_t pattern_count) {
    size_t found = 0;
    for (size_t i = 0; i < pattern_count; i++) {
        size_t plen = strlen(patterns[i]);
        for (size_t j = 0; j + plen <= text_len; j++) {
            bool match = true;
            for (size_t k = 0; k < plen; k++) {
                if (tolower((unsigned char)text[j + k]) !=
                    tolower((unsigned char)patterns[i][k])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                found++;
                break;
            }
        }
    }
    return found;
}

/* ── Opinion markers in user messages ─────────────────────────────── */

static const char *opinion_markers[] = {
    "i think",     "i believe",   "i feel",      "in my opinion",
    "don't you think", "isn't it",    "right?",      "wouldn't you say",
    "i hate",      "i love",      "should we",   "we should",
    "obviously",   "clearly",     "everyone knows",
};
static const size_t opinion_marker_count = sizeof(opinion_markers) / sizeof(opinion_markers[0]);

/* ── Sycophancy check ────────────────────────────────────────────── */

hu_error_t hu_sycophancy_check(const char *response, size_t response_len,
                               const char *user_message, size_t user_len,
                               float threshold,
                               hu_sycophancy_result_t *result) {
    if (!response || !result)
        return HU_ERR_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    if (threshold <= 0.0f)
        threshold = 0.5f;

    size_t agree_hits = ci_count_patterns(response, response_len,
                                          agreement_patterns, agreement_count);
    size_t obseq_hits = ci_count_patterns(response, response_len,
                                          obsequious_patterns, obsequious_count);
    size_t excite_hits = ci_count_patterns(response, response_len,
                                           excitement_patterns, excitement_count);

    result->factor_scores[HU_SYCOPHANCY_UNCRITICAL_AGREEMENT] =
        agree_hits > 0 ? (float)agree_hits / 3.0f : 0.0f;
    result->factor_scores[HU_SYCOPHANCY_OBSEQUIOUSNESS] =
        obseq_hits > 0 ? (float)obseq_hits / 3.0f : 0.0f;
    result->factor_scores[HU_SYCOPHANCY_EXCITEMENT] =
        excite_hits > 0 ? (float)excite_hits / 3.0f : 0.0f;

    /* Boost agreement score when user expressed a strong opinion —
     * agreeing with an opinion is more concerning than agreeing with a fact. */
    if (user_message && user_len > 0 && agree_hits > 0) {
        size_t opinion_hits = ci_count_patterns(user_message, user_len,
                                                opinion_markers, opinion_marker_count);
        if (opinion_hits > 0) {
            float boost = (float)opinion_hits * 0.15f;
            result->factor_scores[HU_SYCOPHANCY_UNCRITICAL_AGREEMENT] += boost;
        }
    }

    for (int f = 0; f < HU_SYCOPHANCY_FACTOR_COUNT; f++) {
        if (result->factor_scores[f] > 1.0f)
            result->factor_scores[f] = 1.0f;
    }

    result->pattern_count = agree_hits + obseq_hits + excite_hits;

    result->total_risk =
        result->factor_scores[HU_SYCOPHANCY_UNCRITICAL_AGREEMENT] * 0.5f +
        result->factor_scores[HU_SYCOPHANCY_OBSEQUIOUSNESS] * 0.3f +
        result->factor_scores[HU_SYCOPHANCY_EXCITEMENT] * 0.2f;

    result->flagged = (result->total_risk >= threshold);
    return HU_OK;
}

/* ── Necessary friction directive ────────────────────────────────── */

hu_error_t hu_sycophancy_build_friction(hu_allocator_t *alloc,
                                        const hu_sycophancy_result_t *result,
                                        const char *user_message, size_t user_len,
                                        char **out, size_t *out_len) {
    if (!alloc || !result || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    if (!result->flagged) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }

    static const char base_directive[] =
        "[ANTI-SYCOPHANCY DIRECTIVE] Your previous response was flagged for "
        "excessive agreement or flattery. Rewrite with genuine engagement: "
        "express your actual perspective, note nuances or counterpoints, "
        "and use calibrated language instead of superlatives. A real friend "
        "pushes back when they disagree — they don't just validate.";

    static const char opinion_addendum[] =
        " The user expressed a personal opinion — engage with it critically "
        "rather than reflexively validating it.";

    bool user_has_opinion = false;
    if (user_message && user_len > 0) {
        size_t opinion_hits = ci_count_patterns(user_message, user_len,
                                                opinion_markers, opinion_marker_count);
        user_has_opinion = (opinion_hits > 0);
    }

    size_t base_len = sizeof(base_directive) - 1;
    size_t add_len = user_has_opinion ? (sizeof(opinion_addendum) - 1) : 0;
    size_t total = base_len + add_len;

    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(buf, base_directive, base_len);
    if (user_has_opinion)
        memcpy(buf + base_len, opinion_addendum, add_len);
    buf[total] = '\0';
    *out = buf;
    *out_len = total;
    return HU_OK;
}
