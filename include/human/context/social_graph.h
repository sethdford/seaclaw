#ifndef HU_CONTEXT_SOCIAL_GRAPH_H
#define HU_CONTEXT_SOCIAL_GRAPH_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include "human/memory/graph.h"
#include "human/persona.h"
#include <stddef.h>

#ifdef HU_ENABLE_SQLITE

/* Build social context from the knowledge graph (entities and relations).
 * Uses hu_graph_build_contact_context when contact_id is provided, else hu_graph_build_context.
 * Caller must free the returned string. */
hu_error_t hu_social_graph_build_context(hu_allocator_t *alloc, hu_graph_t *graph,
                                         const char *query, size_t query_len,
                                         const char *contact_id, size_t contact_id_len,
                                         size_t max_hops, size_t max_chars, char **out,
                                         size_t *out_len);

/* Store a relationship for a contact. Upserts if (contact_id, person_name) exists. */
hu_error_t hu_social_graph_store(hu_allocator_t *alloc, hu_memory_t *memory,
                                 const char *contact_id, size_t cid_len,
                                 const hu_relationship_t *rel);

/* Get all relationships for a contact. Caller must free with hu_social_graph_free. */
hu_error_t hu_social_graph_get(hu_allocator_t *alloc, hu_memory_t *memory,
                               const char *contact_id, size_t cid_len,
                               hu_relationship_t **out, size_t *out_count);

/* Build directive string for prompt injection. Returns NULL if count is 0. Caller must free. */
char *hu_social_graph_build_directive(hu_allocator_t *alloc,
                                      const char *contact_name, size_t name_len,
                                      const hu_relationship_t *rels, size_t count,
                                      size_t *out_len);

/* Free array returned by hu_social_graph_get. */
void hu_social_graph_free(hu_allocator_t *alloc, hu_relationship_t *rels, size_t count);

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_CONTEXT_SOCIAL_GRAPH_H */
