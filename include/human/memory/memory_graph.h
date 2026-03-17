#ifndef HU_MEMORY_GRAPH_MAGMA_H
#define HU_MEMORY_GRAPH_MAGMA_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef enum {
    HU_GRAPH_SEMANTIC = 0,
    HU_GRAPH_TEMPORAL,
    HU_GRAPH_ENTITY,
    HU_GRAPH_CAUSAL
} hu_memory_graph_type_t;

typedef struct hu_memory_edge {
    int64_t source_id;
    int64_t target_id;
    hu_memory_graph_type_t graph_type;
    double weight;
    int64_t created_at;
} hu_memory_edge_t;

typedef struct hu_memory_node {
    int64_t id;
    char content_hash[65]; /* SHA-256 hex */
    char content_preview[256];
    size_t preview_len;
    hu_memory_graph_type_t type;
} hu_memory_node_t;

typedef struct hu_memory_graph {
    hu_allocator_t *alloc;
    sqlite3 *db;
} hu_memory_graph_t;

hu_error_t hu_memory_graph_create(hu_allocator_t *alloc, sqlite3 *db, hu_memory_graph_t *out);
hu_error_t hu_memory_graph_init_tables(hu_memory_graph_t *g);
void hu_memory_graph_deinit(hu_memory_graph_t *g);

hu_error_t hu_memory_graph_add_node(hu_memory_graph_t *g, const char *content, size_t content_len,
                                    int64_t *out_id);

hu_error_t hu_memory_graph_add_edge(hu_memory_graph_t *g, int64_t source, int64_t target,
                                    hu_memory_graph_type_t type, double weight);

hu_error_t hu_memory_graph_traverse(hu_memory_graph_t *g, int64_t start_node,
                                    hu_memory_graph_type_t type, int max_hops,
                                    hu_memory_node_t *results, size_t max_results,
                                    size_t *out_count);

hu_error_t hu_memory_graph_find_bridges(hu_memory_graph_t *g, int64_t node_a, int64_t node_b,
                                        hu_memory_node_t *bridges, size_t max_bridges,
                                        size_t *out_count);

const char *hu_memory_graph_type_name(hu_memory_graph_type_t type);

hu_error_t hu_memory_graph_ingest(hu_memory_graph_t *g, const char *content, size_t content_len,
                                   int64_t timestamp);

hu_error_t hu_memory_graph_build_context(hu_memory_graph_t *g, hu_allocator_t *alloc,
                                          int64_t node_id, int max_hops,
                                          char **out, size_t *out_len);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_MEMORY_GRAPH_MAGMA_H */
