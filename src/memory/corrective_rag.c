#include "human/memory/corrective_rag.h"
#include "human/memory.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

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
    if (!config->memory || !config->memory->vtable || !config->memory->vtable->recall)
        return HU_ERR_NOT_SUPPORTED;

    hu_memory_entry_t *entries = NULL;
    size_t count = 0;
    size_t limit = config->max_web_results > 0 ? config->max_web_results : 32;
    hu_error_t err = config->memory->vtable->recall(
        config->memory->ctx, alloc, query, query_len, limit,
        config->memory->current_session_id, config->memory->current_session_id_len, &entries,
        &count);
    if (err != HU_OK || !entries || count == 0) {
        if (entries) {
            for (size_t i = 0; i < count; i++)
                hu_memory_entry_free_fields(alloc, &entries[i]);
            alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        }
        out->answer = (char *)alloc->alloc(alloc->ctx, 1);
        if (out->answer)
            out->answer[0] = '\0';
        return HU_OK;
    }

    hu_rag_graded_doc_t *graded = (hu_rag_graded_doc_t *)alloc->alloc(
        alloc->ctx, count * sizeof(hu_rag_graded_doc_t));
    if (!graded) {
        for (size_t i = 0; i < count; i++)
            hu_memory_entry_free_fields(alloc, &entries[i]);
        alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        return HU_ERR_OUT_OF_MEMORY;
    }

    char *correction_owned = NULL;
    size_t correction_len = 0;
#ifdef HU_ENABLE_SQLITE
    sqlite3 *db = hu_sqlite_memory_get_db(config->memory);
#endif

    for (size_t i = 0; i < count; i++) {
        const char *doc = entries[i].content;
        size_t doc_len = entries[i].content_len;
        if (!doc)
            doc = "";
        if (doc_len == 0 && doc[0] != '\0')
            doc_len = strlen(doc);

#ifdef HU_ENABLE_SQLITE
        if (db && entries[i].key && entries[i].key_len > 0) {
            if (correction_owned)
                alloc->free(alloc->ctx, correction_owned, correction_len + 1);
            correction_owned = NULL;
            char corr_key[512];
            int n = snprintf(corr_key, sizeof(corr_key), "correction:%.*s",
                            (int)entries[i].key_len, entries[i].key);
            if (n > 0 && (size_t)n < sizeof(corr_key)) {
                sqlite3_stmt *stmt = NULL;
                if (sqlite3_prepare_v2(db, "SELECT content FROM memories WHERE key = ?1 LIMIT 1",
                                       -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, corr_key, n, SQLITE_STATIC);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *c = (const char *)sqlite3_column_text(stmt, 0);
                        if (c) {
                            size_t len = (size_t)sqlite3_column_bytes(stmt, 0);
                            char *copy = (char *)alloc->alloc(alloc->ctx, len + 1);
                            if (copy) {
                                memcpy(copy, c, len);
                                copy[len] = '\0';
                                doc = copy;
                                doc_len = len;
                                correction_owned = copy;
                                correction_len = len;
                            }
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
#endif

        err = hu_crag_grade_document(alloc, query, query_len, doc, doc_len, &graded[i]);
        if (err != HU_OK)
            graded[i].relevance = HU_RAG_IRRELEVANT;

        if (correction_owned) {
            alloc->free(alloc->ctx, correction_owned, correction_len + 1);
            correction_owned = NULL;
        }
    }

    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            int cmp = (int)graded[j].relevance - (int)graded[i].relevance;
            if (cmp > 0 || (cmp == 0 && graded[j].score > graded[i].score)) {
                hu_rag_graded_doc_t tmp = graded[i];
                graded[i] = graded[j];
                graded[j] = tmp;
                hu_memory_entry_t etmp = entries[i];
                entries[i] = entries[j];
                entries[j] = etmp;
            }
        }
    }

    size_t out_cap = 4096;
    char *answer = (char *)alloc->alloc(alloc->ctx, out_cap);
    if (!answer) {
        alloc->free(alloc->ctx, graded, count * sizeof(hu_rag_graded_doc_t));
        for (size_t i = 0; i < count; i++)
            hu_memory_entry_free_fields(alloc, &entries[i]);
        alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t pos = 0;
    for (size_t i = 0; i < count && pos < out_cap - 1; i++) {
        if (graded[i].relevance == HU_RAG_IRRELEVANT &&
            (i == 0 || graded[i - 1].relevance != HU_RAG_IRRELEVANT))
            break;
        const char *src = entries[i].content;
        size_t src_len = entries[i].content_len;
        if (!src)
            continue;
        if (src_len == 0)
            src_len = strlen(src);
        size_t to_copy = src_len;
        if (pos + to_copy >= out_cap - 1)
            to_copy = out_cap - 1 - pos;
        if (to_copy > 0) {
            memcpy(answer + pos, src, to_copy);
            pos += to_copy;
            if (pos < out_cap - 1 && i + 1 < count)
                answer[pos++] = '\n';
        }
    }
    answer[pos] = '\0';
    out->answer = answer;
    out->answer_len = pos;
    out->docs = NULL;
    out->docs_count = 0;

    alloc->free(alloc->ctx, graded, count * sizeof(hu_rag_graded_doc_t));
    for (size_t i = 0; i < count; i++)
        hu_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
    return HU_OK;
#endif
}

void hu_crag_result_free(hu_allocator_t *alloc, hu_crag_result_t *result) {
    if (!alloc || !result) return;
    if (result->answer) { alloc->free(alloc->ctx, result->answer, result->answer_len + 1); result->answer = NULL; }
}
