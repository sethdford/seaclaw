#ifndef HU_CONTEXT_ENGINE_H
#define HU_CONTEXT_ENGINE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Context Engine — pluggable lifecycle for conversation context management.
 *
 * Inspired by OpenClaw's ContextEngine plugin model, adapted to h-uman's
 * vtable architecture. Covers the full context lifecycle: bootstrap,
 * ingest, assemble, compact, after-turn, and subagent context isolation.
 *
 * Default implementation wraps h-uman's existing behavior (memory loader +
 * compaction + anticipatory + speculative). Users can swap in custom engines
 * for RAG-based assembly, graph memory, lossless compression, etc.
 */

typedef struct hu_context_message {
    const char *role;
    size_t role_len;
    const char *content;
    size_t content_len;
    int64_t timestamp;
} hu_context_message_t;

typedef struct hu_context_budget {
    size_t max_tokens;
    size_t reserved_tokens;
    size_t used_tokens;
} hu_context_budget_t;

typedef struct hu_assembled_context {
    char *system_prompt;
    size_t system_prompt_len;
    hu_context_message_t *messages;
    size_t messages_count;
    char *injected_context;
    size_t injected_context_len;
    size_t total_tokens;
    bool owns_messages;
} hu_assembled_context_t;

struct hu_context_engine_vtable;

typedef struct hu_context_engine {
    void *ctx;
    const struct hu_context_engine_vtable *vtable;
} hu_context_engine_t;

typedef struct hu_context_engine_vtable {
    /* Initialize the engine (connect vector DB, load state, etc.). */
    hu_error_t (*bootstrap)(void *ctx, hu_allocator_t *alloc);

    /* Process and index a new message into the engine's store. */
    hu_error_t (*ingest)(void *ctx, hu_allocator_t *alloc, const hu_context_message_t *message);

    /* Build the final context within the given token budget.
     * This is where RAG retrieval, recency selection, anticipatory
     * injection, and speculative prefetching happen. */
    hu_error_t (*assemble)(void *ctx, hu_allocator_t *alloc, const hu_context_budget_t *budget,
                           hu_assembled_context_t *out);

    /* Compress context when approaching token limits.
     * May summarize, prune, or reorganize stored context. */
    hu_error_t (*compact)(void *ctx, hu_allocator_t *alloc, size_t target_tokens);

    /* Post-turn cleanup and bookkeeping. */
    hu_error_t (*after_turn)(void *ctx, hu_allocator_t *alloc, const hu_context_message_t *user_msg,
                             const hu_context_message_t *assistant_msg);

    /* Prepare an isolated context snapshot for a subagent. */
    hu_error_t (*prepare_subagent)(void *ctx, hu_allocator_t *alloc,
                                   hu_context_engine_t *subagent_engine);

    /* Merge subagent results back into the parent context. */
    hu_error_t (*merge_subagent)(void *ctx, hu_allocator_t *alloc,
                                 const hu_context_engine_t *subagent_engine);

    const char *(*get_name)(void *ctx);

    void (*deinit)(void *ctx, hu_allocator_t *alloc);
} hu_context_engine_vtable_t;

/* Create the default (legacy) context engine that wraps existing behavior. */
hu_error_t hu_context_engine_legacy_create(hu_allocator_t *alloc, hu_context_engine_t *out);

/* Free an assembled context. */
void hu_assembled_context_free(hu_allocator_t *alloc, hu_assembled_context_t *ctx);

#endif /* HU_CONTEXT_ENGINE_H */
