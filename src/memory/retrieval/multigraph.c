/* MAGMA-style multi-graph retrieval: semantic, temporal, causal, relational, community. */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include "human/memory/graph.h"
#include "human/memory/retrieval_policy.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const size_t DEFAULT_BUDGET = 2000;

hu_retrieval_policy_t hu_retrieval_policy_default(void) {
    hu_retrieval_policy_t p = {0};
    p.total_budget_chars = DEFAULT_BUDGET;
    p.active_count = 5;
    for (int i = 0; i < 5; i++) {
        p.dims[i].dim = (hu_graph_dimension_t)i;
        p.dims[i].weight = 0.2f;
        p.dims[i].max_chars = (size_t)(DEFAULT_BUDGET * 0.2f);
    }
    return p;
}

hu_retrieval_policy_t hu_retrieval_policy_for_intent(hu_query_intent_t intent) {
    hu_retrieval_policy_t p = {0};
    p.total_budget_chars = DEFAULT_BUDGET;

    switch (intent) {
    case HU_INTENT_FACTUAL:
        p.active_count = 5;
        p.dims[0] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_SEMANTIC, 0.4f,
            (size_t)(DEFAULT_BUDGET * 0.4f)};
        p.dims[1] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_RELATIONAL, 0.3f,
            (size_t)(DEFAULT_BUDGET * 0.3f)};
        p.dims[2] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_COMMUNITY, 0.2f,
            (size_t)(DEFAULT_BUDGET * 0.2f)};
        p.dims[3] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_TEMPORAL, 0.05f,
            (size_t)(DEFAULT_BUDGET * 0.05f)};
        p.dims[4] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_CAUSAL, 0.05f,
            (size_t)(DEFAULT_BUDGET * 0.05f)};
        break;
    case HU_INTENT_TEMPORAL:
        p.active_count = 5;
        p.dims[0] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_TEMPORAL, 0.5f,
            (size_t)(DEFAULT_BUDGET * 0.5f)};
        p.dims[1] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_SEMANTIC, 0.2f,
            (size_t)(DEFAULT_BUDGET * 0.2f)};
        p.dims[2] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_CAUSAL, 0.15f,
            (size_t)(DEFAULT_BUDGET * 0.15f)};
        p.dims[3] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_RELATIONAL, 0.1f,
            (size_t)(DEFAULT_BUDGET * 0.1f)};
        p.dims[4] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_COMMUNITY, 0.05f,
            (size_t)(DEFAULT_BUDGET * 0.05f)};
        break;
    case HU_INTENT_CAUSAL:
        p.active_count = 5;
        p.dims[0] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_CAUSAL, 0.4f,
            (size_t)(DEFAULT_BUDGET * 0.4f)};
        p.dims[1] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_SEMANTIC, 0.25f,
            (size_t)(DEFAULT_BUDGET * 0.25f)};
        p.dims[2] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_TEMPORAL, 0.2f,
            (size_t)(DEFAULT_BUDGET * 0.2f)};
        p.dims[3] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_RELATIONAL, 0.1f,
            (size_t)(DEFAULT_BUDGET * 0.1f)};
        p.dims[4] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_COMMUNITY, 0.05f,
            (size_t)(DEFAULT_BUDGET * 0.05f)};
        break;
    case HU_INTENT_RELATIONAL:
        p.active_count = 5;
        p.dims[0] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_RELATIONAL, 0.4f,
            (size_t)(DEFAULT_BUDGET * 0.4f)};
        p.dims[1] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_SEMANTIC, 0.25f,
            (size_t)(DEFAULT_BUDGET * 0.25f)};
        p.dims[2] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_COMMUNITY, 0.2f,
            (size_t)(DEFAULT_BUDGET * 0.2f)};
        p.dims[3] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_TEMPORAL, 0.1f,
            (size_t)(DEFAULT_BUDGET * 0.1f)};
        p.dims[4] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_CAUSAL, 0.05f,
            (size_t)(DEFAULT_BUDGET * 0.05f)};
        break;
    case HU_INTENT_EXPLORATORY:
        p.active_count = 5;
        p.dims[0] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_COMMUNITY, 0.3f,
            (size_t)(DEFAULT_BUDGET * 0.3f)};
        p.dims[1] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_SEMANTIC, 0.25f,
            (size_t)(DEFAULT_BUDGET * 0.25f)};
        p.dims[2] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_RELATIONAL, 0.2f,
            (size_t)(DEFAULT_BUDGET * 0.2f)};
        p.dims[3] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_TEMPORAL, 0.15f,
            (size_t)(DEFAULT_BUDGET * 0.15f)};
        p.dims[4] = (hu_graph_dimension_config_t){HU_GRAPH_DIM_CAUSAL, 0.1f,
            (size_t)(DEFAULT_BUDGET * 0.1f)};
        break;
    }
    return p;
}

static bool contains_word(const char *query, size_t query_len, const char *word) {
    size_t wlen = strlen(word);
    if (wlen > query_len)
        return false;
    for (size_t i = 0; i <= query_len - wlen; i++) {
        if (i > 0 && (isalnum((unsigned char)query[i - 1]) || query[i - 1] == '_'))
            continue;
        if (i + wlen < query_len &&
            (isalnum((unsigned char)query[i + wlen]) || query[i + wlen] == '_'))
            continue;
        if (strncasecmp(query + i, word, wlen) == 0)
            return true;
    }
    return false;
}

hu_query_intent_t hu_query_classify_intent(const char *query, size_t query_len) {
    if (!query || query_len == 0)
        return HU_INTENT_FACTUAL;

    if (contains_word(query, query_len, "when") || contains_word(query, query_len, "before") ||
        contains_word(query, query_len, "after") || contains_word(query, query_len, "yesterday") ||
        contains_word(query, query_len, "last week") || contains_word(query, query_len, "timeline"))
        return HU_INTENT_TEMPORAL;

    if (contains_word(query, query_len, "why") || contains_word(query, query_len, "because") ||
        contains_word(query, query_len, "caused") || contains_word(query, query_len, "effect") ||
        contains_word(query, query_len, "result") || contains_word(query, query_len, "consequence"))
        return HU_INTENT_CAUSAL;

    if (contains_word(query, query_len, "who") || contains_word(query, query_len, "knows") ||
        contains_word(query, query_len, "friend") || contains_word(query, query_len, "relationship") ||
        contains_word(query, query_len, "connection"))
        return HU_INTENT_RELATIONAL;

    if (contains_word(query, query_len, "what if") || contains_word(query, query_len, "explore") ||
        contains_word(query, query_len, "tell me about") || contains_word(query, query_len, "overview"))
        return HU_INTENT_EXPLORATORY;

    return HU_INTENT_FACTUAL;
}

static const char *dim_header(hu_graph_dimension_t dim) {
    switch (dim) {
    case HU_GRAPH_DIM_SEMANTIC:
        return "## Semantic Context";
    case HU_GRAPH_DIM_TEMPORAL:
        return "## Timeline";
    case HU_GRAPH_DIM_CAUSAL:
        return "## Causal Links";
    case HU_GRAPH_DIM_RELATIONAL:
        return "## Relationships";
    case HU_GRAPH_DIM_COMMUNITY:
        return "## Communities";
    default:
        return "";
    }
}

hu_error_t hu_multigraph_retrieve(hu_allocator_t *alloc, hu_graph_t *graph,
                                   const char *query, size_t query_len,
                                   const hu_retrieval_policy_t *policy,
                                   char **out, size_t *out_len, float *out_score) {
    if (!alloc || !out || !out_len || !out_score)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;
    *out_score = 0.0f;

    if (!graph) {
        return HU_OK;
    }

    if (!policy || policy->active_count == 0) {
        return HU_OK;
    }

    char *merged = NULL;
    size_t merged_cap = 0;
    size_t merged_len = 0;
    float score = 0.0f;

    for (size_t i = 0; i < policy->active_count; i++) {
        const hu_graph_dimension_config_t *dc = &policy->dims[i];
        size_t max_chars = dc->max_chars > 0 ? dc->max_chars
                                             : (size_t)(policy->total_budget_chars * dc->weight);
        if (max_chars == 0)
            max_chars = 256;

        char *buf = NULL;
        size_t buf_len = 0;
        hu_error_t err = HU_OK;

        switch (dc->dim) {
        case HU_GRAPH_DIM_SEMANTIC:
            err = hu_graph_build_context(graph, alloc, query, query_len, 2, max_chars, &buf, &buf_len);
            break;
        case HU_GRAPH_DIM_TEMPORAL:
            err = hu_graph_query_temporal(graph, alloc, 0, INT64_MAX, 10, &buf, &buf_len);
            break;
        case HU_GRAPH_DIM_CAUSAL:
            err = hu_graph_query_causal(graph, alloc, 0, 5, &buf, &buf_len);
            break;
        case HU_GRAPH_DIM_RELATIONAL:
            err = hu_graph_build_context(graph, alloc, query, query_len, 3, max_chars, &buf, &buf_len);
            break;
        case HU_GRAPH_DIM_COMMUNITY:
            err = hu_graph_build_communities(graph, alloc, 5, max_chars, &buf, &buf_len);
            break;
        default:
            break;
        }

        if (err != HU_OK) {
            if (buf)
                alloc->free(alloc->ctx, buf, buf_len + 1);
            continue;
        }

        if (buf && buf_len > 0) {
            score += dc->weight;
            const char *header = dim_header(dc->dim);
            size_t header_len = strlen(header);
            size_t need = merged_len + (merged_len > 0 ? 2 : 0) + header_len + 2 + buf_len + 1;
            if (need > merged_cap) {
                size_t new_cap = need + 512;
                char *n = (char *)alloc->realloc(alloc->ctx, merged, merged_cap, new_cap);
                if (!n) {
                    alloc->free(alloc->ctx, buf, buf_len + 1);
                    if (merged)
                        alloc->free(alloc->ctx, merged, merged_cap);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                merged = n;
                merged_cap = new_cap;
            }
            if (merged_len > 0) {
                merged[merged_len++] = '\n';
                merged[merged_len++] = '\n';
            }
            memcpy(merged + merged_len, header, header_len + 1);
            merged_len += header_len;
            merged[merged_len++] = '\n';
            merged[merged_len++] = '\n';
            memcpy(merged + merged_len, buf, buf_len + 1);
            merged_len += buf_len;
        }
        if (buf)
            alloc->free(alloc->ctx, buf, buf_len + 1);
    }

    *out = merged;
    *out_len = merged_len;
    *out_score = score;
    return HU_OK;
}
