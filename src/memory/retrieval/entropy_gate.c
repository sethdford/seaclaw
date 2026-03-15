#include "human/memory/entropy_gate.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

#define MAX_WORDS 256
#define FILLER_COUNT 9

static const char *const FILLERS[FILLER_COUNT] = {
    "ok", "sure", "thanks", "yes", "no", "the", "a", "an", "is",
};
static const size_t FILLER_LENS[FILLER_COUNT] = {2, 4, 6, 3, 2, 3, 1, 2, 2};

static bool is_filler(const char *word, size_t len) {
    for (int i = 0; i < FILLER_COUNT; i++) {
        if (len == FILLER_LENS[i] && strncasecmp(word, FILLERS[i], len) == 0)
            return true;
    }
    return false;
}

typedef struct {
    const char *start;
    size_t len;
} word_t;

static size_t tokenize(const char *text, size_t text_len, word_t *words) {
    size_t n = 0;
    const char *p = text;
    const char *end = text + text_len;
    while (p < end && n < MAX_WORDS) {
        while (p < end && isspace((unsigned char)*p))
            p++;
        if (p >= end)
            break;
        const char *start = p;
        while (p < end && !isspace((unsigned char)*p))
            p++;
        words[n].start = start;
        words[n].len = (size_t)(p - start);
        if (words[n].len > 0)
            n++;
    }
    return n;
}

static bool word_eq(const word_t *a, const word_t *b) {
    return a->len == b->len && memcmp(a->start, b->start, a->len) == 0;
}

hu_entropy_gate_config_t hu_entropy_gate_config_default(void) {
    return (hu_entropy_gate_config_t){
        .threshold = 0.3,
        .context_budget = 4096,
        .adaptive = true,
    };
}

hu_error_t hu_entropy_compute(const char *text, size_t text_len, double *out_entropy) {
    if (!text || !out_entropy)
        return HU_ERR_INVALID_ARGUMENT;
    *out_entropy = 0.0;
    if (text_len == 0)
        return HU_OK;

    word_t words[MAX_WORDS];
    size_t word_count = tokenize(text, text_len, words);
    if (word_count == 0)
        return HU_OK;

    /* Build list of non-filler words and their counts */
    typedef struct {
        word_t w;
        size_t count;
    } unique_t;
    unique_t uniq[MAX_WORDS];
    size_t uniq_count = 0;
    size_t non_filler_total = 0;

    for (size_t i = 0; i < word_count; i++) {
        if (is_filler(words[i].start, words[i].len))
            continue;
        non_filler_total++;
        size_t j;
        for (j = 0; j < uniq_count; j++) {
            if (word_eq(&words[i], &uniq[j].w)) {
                uniq[j].count++;
                break;
            }
        }
        if (j == uniq_count && uniq_count < MAX_WORDS) {
            uniq[uniq_count].w = words[i];
            uniq[uniq_count].count = 1;
            uniq_count++;
        }
    }

    if (non_filler_total == 0)
        return HU_OK;

    double h = 0.0;
    for (size_t i = 0; i < uniq_count; i++) {
        double p = (double)uniq[i].count / (double)non_filler_total;
        if (p > 0.0)
            h -= p * log2(p);
    }

    double max_h = log2((double)non_filler_total);
    if (max_h <= 0.0)
        *out_entropy = 0.0;
    else
        *out_entropy = h / max_h;

    /* Short text penalty: <5 non-filler words */
    if (non_filler_total < 5)
        *out_entropy *= (double)non_filler_total / 5.0;

    if (*out_entropy > 1.0)
        *out_entropy = 1.0;
    if (*out_entropy < 0.0)
        *out_entropy = 0.0;
    return HU_OK;
}

hu_error_t hu_entropy_gate_filter(const hu_entropy_gate_config_t *config,
                                   hu_memory_chunk_t *chunks, size_t chunk_count,
                                   size_t *out_passed_count) {
    if (!config || !chunks || !out_passed_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_passed_count = 0;

    /* Compute entropy for each chunk */
    size_t total_tokens = 0;
    for (size_t i = 0; i < chunk_count; i++) {
        double e = 0.0;
        hu_error_t err = hu_entropy_compute(chunks[i].text, chunks[i].text_len, &e);
        if (err != HU_OK)
            return err;
        chunks[i].entropy = e;
        total_tokens += chunks[i].text_len; /* approximate tokens as chars */
    }

    double thresh = config->threshold;
    if (config->adaptive && config->context_budget > 0 && total_tokens > config->context_budget) {
        double excess = (double)(total_tokens - config->context_budget) / (double)config->context_budget;
        thresh += excess * 0.2; /* increase threshold when over budget */
        if (thresh > 1.0)
            thresh = 1.0;
    }

    for (size_t i = 0; i < chunk_count; i++) {
        chunks[i].passed = (chunks[i].entropy >= thresh);
        if (chunks[i].passed)
            (*out_passed_count)++;
    }
    return HU_OK;
}

hu_error_t hu_entropy_coarsen(hu_allocator_t *alloc,
                              const hu_memory_chunk_t *chunks, size_t chunk_count,
                              char *summary, size_t summary_max, size_t *summary_len) {
    if (!alloc || !chunks || !summary || summary_max == 0 || !summary_len)
        return HU_ERR_INVALID_ARGUMENT;
    *summary_len = 0;
    summary[0] = '\0';

    /* Collect unique words from FAILED (low-entropy) chunks only */
    typedef struct {
        const char *start;
        size_t len;
    } coarse_word_t;
    coarse_word_t seen[MAX_WORDS];
    size_t seen_count = 0;

    for (size_t c = 0; c < chunk_count && seen_count < MAX_WORDS; c++) {
        if (chunks[c].passed)
            continue; /* only failed chunks */
        const char *text = chunks[c].text;
        size_t text_len = chunks[c].text_len;
        const char *p = text;
        const char *end = text + text_len;
        while (p < end && seen_count < MAX_WORDS) {
            while (p < end && isspace((unsigned char)*p))
                p++;
            if (p >= end)
                break;
            const char *start = p;
            while (p < end && !isspace((unsigned char)*p))
                p++;
            size_t wlen = (size_t)(p - start);
            if (wlen == 0)
                continue;
            /* Check if already seen */
            bool found = false;
            for (size_t s = 0; s < seen_count; s++) {
                if (seen[s].len == wlen && memcmp(seen[s].start, start, wlen) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                seen[seen_count].start = start;
                seen[seen_count].len = wlen;
                seen_count++;
            }
        }
    }

    /* Build summary: concatenate unique words with spaces */
    size_t pos = 0;
    for (size_t i = 0; i < seen_count && pos < summary_max; i++) {
        if (pos > 0) {
            if (pos + 1 >= summary_max)
                break;
            summary[pos++] = ' ';
        }
        size_t to_copy = seen[i].len;
        if (pos + to_copy >= summary_max)
            to_copy = summary_max - pos - 1;
        memcpy(summary + pos, seen[i].start, to_copy);
        pos += to_copy;
    }
    summary[pos] = '\0';
    *summary_len = pos;
    return HU_OK;
}
