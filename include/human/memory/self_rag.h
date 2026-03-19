#ifndef HU_MEMORY_SELF_RAG_H
#define HU_MEMORY_SELF_RAG_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum hu_retrieval_decision {
    HU_SRAG_NO_RETRIEVAL = 0,
    HU_SRAG_RETRIEVE,
    HU_SRAG_RETRIEVE_AND_VERIFY
} hu_retrieval_decision_t;

typedef struct hu_srag_config {
    bool enabled;
    double confidence_threshold;
    hu_provider_t *provider;
} hu_srag_config_t;

typedef struct hu_srag_assessment {
    hu_retrieval_decision_t decision;
    double confidence;
    bool is_factual_query;
    bool is_personal_query;
    bool is_creative_query;
    bool has_temporal_marker;
} hu_srag_assessment_t;

hu_srag_config_t hu_srag_config_default(void);

hu_error_t hu_srag_should_retrieve(hu_allocator_t *alloc, const hu_srag_config_t *config,
                                   const char *query, size_t query_len,
                                   const char *history, size_t history_len,
                                   hu_srag_assessment_t *out);

hu_error_t hu_srag_verify_relevance(hu_allocator_t *alloc, const hu_srag_config_t *config,
                                    const char *query, size_t query_len,
                                    const char *retrieved, size_t retrieved_len,
                                    double *relevance_score, bool *should_use);

#endif /* HU_MEMORY_SELF_RAG_H */
