#ifndef HU_CONTEXT_ENGINE_RAG_H
#define HU_CONTEXT_ENGINE_RAG_H

#include "human/context_engine.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include "human/provider.h"

/*
 * RAG Context Engine — intelligent context assembly using retrieval-augmented
 * generation, anticipatory emotional injection, and speculative prefetching.
 *
 * Wires together h-uman's existing memory subsystems through the
 * hu_context_engine_t vtable for pluggable context management:
 *
 * - Recency: keeps the most recent N messages
 * - RAG retrieval: queries memory backend with the user's latest message
 * - Anticipatory: injects emotional context predictions
 * - Speculative: checks pre-computed response cache
 * - Compaction: summarizes old messages when approaching token limit
 */

typedef struct hu_context_engine_rag_config {
    hu_memory_t *memory;     /* optional: memory backend for RAG retrieval */
    hu_provider_t *provider; /* optional: for compaction summarization */
    const char *model_name;  /* model for provider calls */
    size_t model_name_len;
    size_t max_recency_messages; /* max recent messages to keep (default 20) */
    size_t rag_result_limit;     /* max RAG results to inject (default 5) */
    double rag_min_relevance;    /* minimum relevance threshold (default 0.3) */
} hu_context_engine_rag_config_t;

/* Create a RAG-based context engine. All pointers are borrowed (not owned). */
hu_error_t hu_context_engine_rag_create(hu_allocator_t *alloc,
                                        const hu_context_engine_rag_config_t *config,
                                        hu_context_engine_t *out);

#endif /* HU_CONTEXT_ENGINE_RAG_H */
