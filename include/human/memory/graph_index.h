#ifndef HU_MEMORY_GRAPH_INDEX_H
#define HU_MEMORY_GRAPH_INDEX_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Multi-dimensional memory graph index (MAGMA-inspired).
 *
 * Extends flat vector-similarity retrieval with orthogonal graph dimensions:
 *   - Temporal: orders memories by recency/sequence
 *   - Entity: links memories mentioning the same entities (people, places, topics)
 *   - Causal: links memories where one led to another (action → result)
 *
 * The index sits alongside the existing memory vtable — it's queried after
 * initial recall to re-rank and augment results with graph-connected memories.
 *
 * Reference: arXiv:2601.03236 (MAGMA)
 */

#define HU_GRAPH_MAX_ENTITIES_PER_ENTRY 8
#define HU_GRAPH_MAX_ENTITY_LEN         64
#define HU_GRAPH_MAX_ENTRIES            1024

typedef struct hu_graph_edge {
    uint32_t target_idx;
    float weight;
} hu_graph_edge_t;

typedef struct hu_graph_node {
    char *memory_key; /* borrowed from memory entry */
    size_t memory_key_len;
    int64_t timestamp;

    /* Entity dimension */
    char entities[HU_GRAPH_MAX_ENTITIES_PER_ENTRY][HU_GRAPH_MAX_ENTITY_LEN];
    size_t entity_count;

    /* Temporal edges (prev/next in sequence) */
    int32_t temporal_prev; /* -1 if none */
    int32_t temporal_next; /* -1 if none */

    /* Entity edges (same entity mentioned) */
    hu_graph_edge_t entity_edges[HU_GRAPH_MAX_ENTITIES_PER_ENTRY];
    size_t entity_edge_count;

    /* Causal edges (this memory caused/led-to another) */
    int32_t causal_next; /* -1 if none */
} hu_graph_node_t;

typedef struct hu_graph_index {
    hu_allocator_t *alloc;
    hu_graph_node_t *nodes;
    size_t node_count;
    size_t node_cap;
} hu_graph_index_t;

hu_error_t hu_graph_index_init(hu_graph_index_t *idx, hu_allocator_t *alloc);
void hu_graph_index_deinit(hu_graph_index_t *idx);

/** Add a memory entry to the graph index. Extracts entities from content. */
hu_error_t hu_graph_index_add(hu_graph_index_t *idx, const char *key, size_t key_len,
                              const char *content, size_t content_len, int64_t timestamp);

/** Re-rank recall results by combining vector similarity with graph connectivity.
 * scores: in/out array of scores for each result (modified in-place).
 * keys/count: the recall result keys to re-rank. */
hu_error_t hu_graph_index_rerank(const hu_graph_index_t *idx, const char *query, size_t query_len,
                                 const char **keys, size_t *key_lens, double *scores, size_t count);

/** Find temporally adjacent memories to a given key. */
hu_error_t hu_graph_index_temporal_neighbors(const hu_graph_index_t *idx, const char *key,
                                             size_t key_len, const char **out_prev,
                                             const char **out_next);

/** Find entity-connected memories for a given key. */
hu_error_t hu_graph_index_entity_neighbors(const hu_graph_index_t *idx, const char *key,
                                           size_t key_len, const char **out_keys, size_t *out_count,
                                           size_t max_results);

#endif /* HU_MEMORY_GRAPH_INDEX_H */
