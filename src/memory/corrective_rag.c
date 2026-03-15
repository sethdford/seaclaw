#include "human/memory/corrective_rag.h"
#include <string.h>
#include <ctype.h>

static double word_overlap(const char *a, size_t alen, const char *b, size_t blen) {
    int matches = 0, words = 0;
    size_t i = 0;
    while (i < alen) {
        while (i < alen && !isalpha((unsigned char)a[i])) i++;
        if (i >= alen) break;
        size_t ws = i;
        while (i < alen && isalpha((unsigned char)a[i])) i++;
        size_t wlen = i - ws;
        words++;
        for (size_t j = 0; j + wlen <= blen; j++) {
            if (j > 0 && isalpha((unsigned char)b[j-1])) continue;
            if (j + wlen < blen && isalpha((unsigned char)b[j+wlen])) continue;
            bool match = true;
            for (size_t k = 0; k < wlen; k++) {
                char ca = a[ws+k]; char cb = b[j+k];
                if (ca >= 'A' && ca <= 'Z') ca += 32;
                if (cb >= 'A' && cb <= 'Z') cb += 32;
                if (ca != cb) { match = false; break; }
            }
            if (match) { matches++; break; }
        }
    }
    return words > 0 ? (double)matches / (double)words : 0.0;
}

hu_error_t hu_crag_grade_document(hu_allocator_t *alloc, const char *query, size_t query_len, const char *doc, size_t doc_len, hu_rag_graded_doc_t *out) {
    if (!alloc || !query || !doc || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->content = doc;
    out->content_len = doc_len;
    out->score = word_overlap(query, query_len, doc, doc_len);
    if (out->score >= 0.5) out->relevance = HU_RAG_RELEVANT;
    else if (out->score >= 0.2) out->relevance = HU_RAG_AMBIGUOUS;
    else out->relevance = HU_RAG_IRRELEVANT;
    return HU_OK;
}

hu_error_t hu_crag_retrieve(hu_allocator_t *alloc, const hu_crag_config_t *config, const char *query, size_t query_len, hu_crag_result_t *out) {
    if (!alloc || !config || !query || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
#ifdef HU_IS_TEST
    (void)query_len;
    const char *ans = "Mock CRAG answer";
    size_t alen = strlen(ans);
    out->answer = alloc->alloc(alloc->ctx, alen + 1);
    if (!out->answer) return HU_ERR_OUT_OF_MEMORY;
    memcpy(out->answer, ans, alen + 1);
    out->answer_len = alen;
    return HU_OK;
#else
    (void)query_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

void hu_crag_result_free(hu_allocator_t *alloc, hu_crag_result_t *result) {
    if (!alloc || !result) return;
    if (result->answer) { alloc->free(alloc->ctx, result->answer, result->answer_len + 1); result->answer = NULL; }
}
