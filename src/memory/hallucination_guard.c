#include "human/memory/hallucination_guard.h"
#include "human/core/string.h"
#include <ctype.h>
#include <string.h>

/* ── Claim extraction patterns ───────────────────────────────────── */

static const char *claim_markers[] = {
    "i remember when you",  "i remember you",      "you told me",
    "you mentioned",        "you said",             "we talked about",
    "last time you",        "you once",             "as you shared",
    "you previously",       "from what you said",   "you brought up",
    "when you described",   "you explained",        "i recall you",
};
static const size_t claim_marker_count = sizeof(claim_markers) / sizeof(claim_markers[0]);

static bool ci_starts_with(const char *haystack, size_t hay_len,
                           const char *needle, size_t needle_len) {
    if (hay_len < needle_len)
        return false;
    for (size_t i = 0; i < needle_len; i++) {
        if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
            return false;
    }
    return true;
}

static size_t find_sentence_end(const char *text, size_t start, size_t text_len) {
    for (size_t i = start; i < text_len; i++) {
        if (text[i] == '.' || text[i] == '!' || text[i] == '?' || text[i] == '\n')
            return i + 1;
    }
    return text_len;
}

hu_error_t hu_hallucination_extract_claims(const char *response, size_t response_len,
                                           hu_hallucination_result_t *result) {
    if (!response || !result)
        return HU_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

    for (size_t pos = 0; pos < response_len && result->claim_count < HU_HALLUCINATION_MAX_CLAIMS;) {
        bool found = false;
        for (size_t m = 0; m < claim_marker_count; m++) {
            size_t mlen = strlen(claim_markers[m]);
            if (ci_starts_with(response + pos, response_len - pos, claim_markers[m], mlen)) {
                size_t end = find_sentence_end(response, pos, response_len);
                size_t clen = end - pos;
                if (clen > HU_HALLUCINATION_CLAIM_MAX_LEN - 1)
                    clen = HU_HALLUCINATION_CLAIM_MAX_LEN - 1;
                hu_memory_claim_t *c = &result->claims[result->claim_count];
                memcpy(c->text, response + pos, clen);
                c->text[clen] = '\0';
                c->text_len = clen;
                c->status = HU_CLAIM_UNVERIFIED;
                c->confidence = 0.0f;
                result->claim_count++;
                result->unverified_count++;
                pos = end;
                found = true;
                break;
            }
        }
        if (!found)
            pos++;
    }
    result->needs_rewrite = (result->unverified_count > 0);
    return HU_OK;
}

/* ── Claim verification ──────────────────────────────────────────── */

static const char *negation_words[] = {
    " not ", " never ", " didn't ", " don't ", " doesn't ",
    " wasn't ", " weren't ", " no ", " nor ", " neither ",
};
static const size_t negation_word_count = sizeof(negation_words) / sizeof(negation_words[0]);

static bool hg_has_negation_mismatch(const char *claim, size_t claim_len,
                                     const char *memory_text, size_t memory_len) {
    bool claim_negated = false;
    bool memory_negated = false;
    for (size_t n = 0; n < negation_word_count; n++) {
        size_t nlen = strlen(negation_words[n]);
        for (size_t j = 0; j + nlen <= claim_len; j++) {
            bool match = true;
            for (size_t k = 0; k < nlen; k++) {
                if (tolower((unsigned char)claim[j + k]) !=
                    tolower((unsigned char)negation_words[n][k])) {
                    match = false;
                    break;
                }
            }
            if (match) { claim_negated = true; break; }
        }
        for (size_t j = 0; j + nlen <= memory_len; j++) {
            bool match = true;
            for (size_t k = 0; k < nlen; k++) {
                if (tolower((unsigned char)memory_text[j + k]) !=
                    tolower((unsigned char)negation_words[n][k])) {
                    match = false;
                    break;
                }
            }
            if (match) { memory_negated = true; break; }
        }
    }
    return claim_negated != memory_negated;
}

hu_error_t hu_hallucination_verify_claims(hu_hallucination_result_t *result,
                                          hu_memory_t *memory,
                                          hu_allocator_t *alloc) {
    if (!result)
        return HU_ERR_INVALID_ARGUMENT;
    if (!memory || !memory->vtable || !memory->vtable->recall) {
        /* Without a usable memory backend we cannot verify claims, so clear the
         * rewrite flag to avoid blindly hedging every "I remember" statement. */
        result->needs_rewrite = false;
        return HU_OK;
    }

    result->verified_count = 0;
    result->unverified_count = 0;
    result->contradicted_count = 0;

    for (size_t i = 0; i < result->claim_count; i++) {
        hu_memory_claim_t *c = &result->claims[i];
        hu_memory_entry_t *entries = NULL;
        size_t entry_count = 0;

        hu_error_t err = memory->vtable->recall(
            memory->ctx, alloc, c->text, c->text_len, 1, NULL, 0, &entries, &entry_count);

        if (err == HU_OK && entries && entry_count > 0) {
            const char *mem_text = entries[0].content;
            size_t mem_len = entries[0].content_len;
            if (mem_text && mem_len > 0 &&
                hg_has_negation_mismatch(c->text, c->text_len, mem_text, mem_len)) {
                c->status = HU_CLAIM_CONTRADICTED;
                c->confidence = 0.7f;
                result->contradicted_count++;
            } else {
                c->status = HU_CLAIM_VERIFIED;
                c->confidence = 0.8f;
                result->verified_count++;
            }
            for (size_t ei = 0; ei < entry_count; ei++)
                hu_memory_entry_free_fields(alloc, &entries[ei]);
            alloc->free(alloc->ctx, entries, entry_count * sizeof(hu_memory_entry_t));
        } else {
            c->status = HU_CLAIM_UNVERIFIED;
            c->confidence = 0.0f;
            result->unverified_count++;
        }
    }
    result->needs_rewrite = (result->unverified_count > 0 || result->contradicted_count > 0);
    return HU_OK;
}

/* ── Rewrite unverified claims ───────────────────────────────────── */

static const char *hedging_prefix = "I think you might have mentioned";

hu_error_t hu_hallucination_rewrite(hu_allocator_t *alloc,
                                    const char *response, size_t response_len,
                                    const hu_hallucination_result_t *result,
                                    char **out, size_t *out_len) {
    if (!alloc || !response || !result || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (!result->needs_rewrite) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }

    size_t cap = response_len + result->claim_count * 64;
    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    size_t w = 0;
    size_t pos = 0;

    for (size_t i = 0; i < result->claim_count && pos < response_len; i++) {
        const hu_memory_claim_t *c = &result->claims[i];
        if (c->status == HU_CLAIM_VERIFIED)
            continue;

        const char *loc = strstr(response + pos, c->text);
        if (!loc)
            continue;

        size_t offset = (size_t)(loc - response);
        size_t copy_before = offset - pos;
        if (w + copy_before < cap) {
            memcpy(buf + w, response + pos, copy_before);
            w += copy_before;
        }

        if (c->status == HU_CLAIM_CONTRADICTED) {
            pos = offset + c->text_len;
            continue;
        }

        /* Hedge unverified claim */
        size_t hlen = strlen(hedging_prefix);
        if (w + hlen + 5 < cap) {
            memcpy(buf + w, hedging_prefix, hlen);
            w += hlen;
            buf[w++] = '.';
            buf[w++] = ' ';
        }
        pos = offset + c->text_len;
    }

    if (pos < response_len && w + (response_len - pos) < cap) {
        memcpy(buf + w, response + pos, response_len - pos);
        w += response_len - pos;
    }
    buf[w] = '\0';

    /* Reallocate to exact size for clean ASan tracking */
    char *exact = (char *)alloc->alloc(alloc->ctx, w + 1);
    if (!exact) {
        alloc->free(alloc->ctx, buf, cap);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(exact, buf, w + 1);
    alloc->free(alloc->ctx, buf, cap);
    *out = exact;
    *out_len = w;
    return HU_OK;
}

/* ── Full pipeline ───────────────────────────────────────────────── */

hu_error_t hu_hallucination_guard(hu_allocator_t *alloc,
                                  const char *response, size_t response_len,
                                  hu_memory_t *memory,
                                  char **out, size_t *out_len) {
    if (!alloc || !response || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    hu_hallucination_result_t result = {0};
    hu_error_t err = hu_hallucination_extract_claims(response, response_len, &result);
    if (err != HU_OK)
        return err;

    if (result.claim_count == 0) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }

    err = hu_hallucination_verify_claims(&result, memory, alloc);
    if (err != HU_OK)
        return err;

    if (!result.needs_rewrite) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }

    return hu_hallucination_rewrite(alloc, response, response_len, &result, out, out_len);
}
