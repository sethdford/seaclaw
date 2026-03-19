#ifndef HU_MEMORY_ADAPTIVE_RAG_H
#define HU_MEMORY_ADAPTIVE_RAG_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

typedef enum hu_rag_strategy {
    HU_RAG_NONE = 0,
    HU_RAG_KEYWORD,
    HU_RAG_SEMANTIC,
    HU_RAG_HYBRID,
    HU_RAG_GRAPH,
    HU_RAG_CORRECTIVE,
    HU_RAG_STRATEGY_COUNT
} hu_rag_strategy_t;

typedef struct hu_rag_query_features {
    size_t word_count;
    bool has_entity_names;
    bool has_temporal_marker;
    bool has_relationship_query;
    bool is_factual;
    bool is_personal;
    double avg_word_length;
} hu_rag_query_features_t;

typedef struct hu_adaptive_rag {
    hu_allocator_t *alloc;
#ifdef HU_ENABLE_SQLITE
    sqlite3 *db;
#else
    void *db;
#endif
    double strategy_weights[HU_RAG_STRATEGY_COUNT];
    size_t strategy_uses[HU_RAG_STRATEGY_COUNT];
} hu_adaptive_rag_t;

hu_error_t hu_adaptive_rag_create(hu_allocator_t *alloc,
#ifdef HU_ENABLE_SQLITE
                                  sqlite3 *db,
#else
                                  void *db,
#endif
                                  hu_adaptive_rag_t *out);

void hu_adaptive_rag_deinit(hu_adaptive_rag_t *rag);

hu_rag_strategy_t hu_adaptive_rag_select(hu_adaptive_rag_t *rag,
                                         const char *query, size_t query_len);

hu_error_t hu_adaptive_rag_record_outcome(hu_adaptive_rag_t *rag,
                                          hu_rag_strategy_t strategy,
                                          double quality_score);

hu_error_t hu_adaptive_rag_extract_features(const char *query, size_t query_len,
                                            hu_rag_query_features_t *features);

const char *hu_rag_strategy_str(hu_rag_strategy_t strategy);

#endif /* HU_MEMORY_ADAPTIVE_RAG_H */
