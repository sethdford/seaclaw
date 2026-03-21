#ifndef HU_COGNITION_SKILL_ROUTING_H
#define HU_COGNITION_SKILL_ROUTING_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations to avoid circular deps */
struct hu_skillforge;
struct hu_skill;

typedef struct hu_skill_route {
    const struct hu_skill *skill;
    float semantic_score;
    float keyword_score;
    float experience_weight;
    float emotional_boost;
    float combined_score;
} hu_skill_route_t;

#define HU_SKILL_BLEND_MAX 3

typedef struct hu_skill_blend {
    hu_skill_route_t routes[HU_SKILL_BLEND_MAX];
    size_t count; /* 1–3 */
} hu_skill_blend_t;

/* Semantic routing context (attached to skillforge). */
typedef struct hu_skill_routing_ctx {
    float *skill_embeddings;  /* flattened: skills_count * embed_dims */
    size_t skills_count;
    size_t embed_dims;
    bool initialized;
} hu_skill_routing_ctx_t;

/* Initialize the routing context (zeroes). */
void hu_skill_routing_init(hu_skill_routing_ctx_t *ctx);

/* Free embedding data. */
void hu_skill_routing_deinit(hu_skill_routing_ctx_t *ctx, hu_allocator_t *alloc);

/* Embed all skill descriptions into the routing context.
 * embed_fn: function that embeds text into a float vector.
 * Returns HU_OK on success; ctx->initialized = true. */
typedef hu_error_t (*hu_embed_fn)(void *embed_ctx, hu_allocator_t *alloc,
                                   const char *text, size_t text_len,
                                   float **out_vec, size_t *out_dims);

hu_error_t hu_skill_routing_embed_catalog(hu_skill_routing_ctx_t *ctx,
                                           hu_allocator_t *alloc,
                                           const struct hu_skill *skills,
                                           size_t skills_count,
                                           hu_embed_fn embed_fn, void *embed_ctx);

/* Route a user message to the best skill(s).
 * keyword_scores: optional array of keyword scores per skill (NULL = ignore).
 * experience_weights: optional array per skill from evolving cognition (NULL = all 1.0).
 * emotional_intensity: from emotional cognition (0 = no boost).
 * Returns top routes in blend (up to blend_max). */
hu_error_t hu_skill_routing_route(const hu_skill_routing_ctx_t *ctx,
                                   hu_allocator_t *alloc,
                                   const char *user_msg, size_t user_msg_len,
                                   hu_embed_fn embed_fn, void *embed_ctx,
                                   const struct hu_skill *skills, size_t skills_count,
                                   const float *keyword_scores,
                                   const double *experience_weights,
                                   float emotional_intensity,
                                   hu_skill_blend_t *out);

/* Build a catalog string from a skill blend (primary + optional secondaries).
 * Returns HU_OK with *out=NULL if no routes. Caller owns returned string. */
hu_error_t hu_skill_routing_build_catalog(hu_allocator_t *alloc,
                                           const hu_skill_blend_t *blend,
                                           const struct hu_skill *skills,
                                           size_t skills_count,
                                           size_t top_k,
                                           char **out, size_t *out_len);

/* Compute cosine similarity between two float vectors. */
float hu_cosine_similarity_f(const float *a, const float *b, size_t dims);

#endif /* HU_COGNITION_SKILL_ROUTING_H */
