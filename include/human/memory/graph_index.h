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

/* ── Spreading activation (SYNAPSE-inspired, arXiv:2501.xxxxx) ─────── */

typedef struct hu_spread_activation_config {
    double initial_energy; /* energy given to seed nodes (default: 1.0) */
    double decay_factor;   /* energy decay per hop (default: 0.5) */
    double threshold;      /* minimum energy to keep propagating (default: 0.05) */
    uint32_t max_hops;     /* max propagation depth (default: 3) */
    size_t max_activated;  /* cap on returned nodes (default: 16) */
} hu_spread_activation_config_t;

typedef struct hu_activated_node {
    uint32_t node_idx;
    double energy;
} hu_activated_node_t;

void hu_spread_activation_config_default(hu_spread_activation_config_t *cfg);

/**
 * Spread activation energy from seed nodes through entity + temporal edges.
 * Returns the top-K activated nodes sorted by descending energy.
 * out_nodes must be pre-allocated to at least max_activated entries.
 */
hu_error_t hu_graph_index_spread_activation(const hu_graph_index_t *idx,
                                            const hu_spread_activation_config_t *cfg,
                                            const uint32_t *seed_indices, size_t seed_count,
                                            hu_activated_node_t *out_nodes, size_t *out_count);

/* ── Hierarchical topic clusters (System-2 traversal) ───────────────── */

typedef struct hu_topic_cluster {
    char label[64];
    uint32_t *member_indices;
    size_t member_count;
    size_t member_cap;
    double centroid_score;
} hu_topic_cluster_t;

typedef struct hu_graph_hierarchy {
    hu_allocator_t *alloc;
    hu_topic_cluster_t *clusters;
    size_t cluster_count;
    size_t cluster_cap;
} hu_graph_hierarchy_t;

hu_error_t hu_graph_hierarchy_init(hu_graph_hierarchy_t *h, hu_allocator_t *alloc);
void hu_graph_hierarchy_deinit(hu_graph_hierarchy_t *h);

/** Cluster graph nodes by dominant shared entity; empty-entity nodes go to "(misc)". */
hu_error_t hu_graph_hierarchy_build(hu_graph_hierarchy_t *h, const hu_graph_index_t *idx);

/** Top-down: score clusters by query term overlap with labels, return member node indices. */
hu_error_t hu_graph_hierarchy_traverse(const hu_graph_hierarchy_t *h, const hu_graph_index_t *idx,
                                       const char *query, size_t query_len, uint32_t *out_indices,
                                       size_t *out_count, size_t max_results);

#endif /* HU_MEMORY_GRAPH_INDEX_H */
