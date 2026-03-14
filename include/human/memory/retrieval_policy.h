#ifndef HU_RETRIEVAL_POLICY_H
#define HU_RETRIEVAL_POLICY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory/graph.h"
#include <stddef.h>

typedef enum hu_graph_dimension {
    HU_GRAPH_DIM_SEMANTIC,
    HU_GRAPH_DIM_TEMPORAL,
    HU_GRAPH_DIM_CAUSAL,
    HU_GRAPH_DIM_RELATIONAL,
    HU_GRAPH_DIM_COMMUNITY,
    HU_GRAPH_DIM_COUNT,
} hu_graph_dimension_t;

typedef struct hu_graph_dimension_config {
    hu_graph_dimension_t dim;
    float weight;
    size_t max_chars;
} hu_graph_dimension_config_t;

typedef struct hu_retrieval_policy {
    hu_graph_dimension_config_t dims[HU_GRAPH_DIM_COUNT];
    size_t active_count;
    size_t total_budget_chars;
} hu_retrieval_policy_t;

typedef enum hu_query_intent {
    HU_INTENT_FACTUAL,
    HU_INTENT_TEMPORAL,
    HU_INTENT_CAUSAL,
    HU_INTENT_RELATIONAL,
    HU_INTENT_EXPLORATORY,
} hu_query_intent_t;

hu_retrieval_policy_t hu_retrieval_policy_default(void);
hu_retrieval_policy_t hu_retrieval_policy_for_intent(hu_query_intent_t intent);
hu_query_intent_t hu_query_classify_intent(const char *query, size_t query_len);

hu_error_t hu_multigraph_retrieve(hu_allocator_t *alloc, hu_graph_t *graph,
                                   const char *query, size_t query_len,
                                   const hu_retrieval_policy_t *policy,
                                   char **out, size_t *out_len, float *out_score);

#endif
