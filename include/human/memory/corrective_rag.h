#ifndef HU_CORRECTIVE_RAG_H
#define HU_CORRECTIVE_RAG_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stdbool.h>
#include <stddef.h>
typedef enum { HU_RAG_RELEVANT=0, HU_RAG_AMBIGUOUS, HU_RAG_IRRELEVANT } hu_rag_relevance_t;
typedef struct hu_rag_graded_doc { const char *content; size_t content_len; double score; hu_rag_relevance_t relevance; } hu_rag_graded_doc_t;
typedef struct hu_crag_config { hu_memory_t *memory; double relevance_threshold; double ambiguity_threshold; size_t max_web_results; } hu_crag_config_t;
typedef struct hu_crag_result { char *answer; size_t answer_len; hu_rag_graded_doc_t *docs; size_t docs_count; bool used_web_search; } hu_crag_result_t;
hu_error_t hu_crag_retrieve(hu_allocator_t *alloc, const hu_crag_config_t *config, const char *query, size_t query_len, hu_crag_result_t *out);
hu_error_t hu_crag_grade_document(hu_allocator_t *alloc, const char *query, size_t query_len, const char *doc, size_t doc_len, hu_rag_graded_doc_t *out);
void hu_crag_result_free(hu_allocator_t *alloc, hu_crag_result_t *result);
#endif
