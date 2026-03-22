#include "human/cognition/skill_routing.h"
#include "human/skillforge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hu_skill_routing_init(hu_skill_routing_ctx_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

void hu_skill_routing_deinit(hu_skill_routing_ctx_t *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc) return;
    if (ctx->skill_embeddings) {
        alloc->free(alloc->ctx, ctx->skill_embeddings,
                    ctx->skills_count * ctx->embed_dims * sizeof(float));
        ctx->skill_embeddings = NULL;
    }
    ctx->skills_count = 0;
    ctx->embed_dims = 0;
    ctx->initialized = false;
}

float hu_cosine_similarity_f(const float *a, const float *b, size_t dims) {
    if (!a || !b || dims == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < dims; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-9f) return 0.0f;
    return dot / denom;
}

hu_error_t hu_skill_routing_embed_catalog(hu_skill_routing_ctx_t *ctx,
                                           hu_allocator_t *alloc,
                                           const hu_skill_t *skills,
                                           size_t skills_count,
                                           hu_embed_fn embed_fn, void *embed_ctx) {
    if (!ctx || !alloc || !skills || !embed_fn || skills_count == 0)
        return HU_ERR_INVALID_ARGUMENT;

    /* Embed the first skill to discover dimensions */
    char text_buf[512];
    int tlen = snprintf(text_buf, sizeof(text_buf), "%s %s",
                        skills[0].name ? skills[0].name : "",
                        skills[0].description ? skills[0].description : "");
    if (tlen < 0) tlen = 0;

    float *first_vec = NULL;
    size_t dims = 0;
    hu_error_t err = embed_fn(embed_ctx, alloc, text_buf, (size_t)tlen, &first_vec, &dims);
    if (err != HU_OK) return err;
    if (dims == 0 || !first_vec) {
        if (first_vec) alloc->free(alloc->ctx, first_vec, 0);
        return HU_ERR_INVALID_ARGUMENT;
    }

    /* Allocate flat embedding matrix */
    size_t total = skills_count * dims * sizeof(float);
    float *embeddings = alloc->alloc(alloc->ctx, total);
    if (!embeddings) {
        alloc->free(alloc->ctx, first_vec, dims * sizeof(float));
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(embeddings, first_vec, dims * sizeof(float));
    alloc->free(alloc->ctx, first_vec, dims * sizeof(float));

    /* Embed remaining skills */
    for (size_t i = 1; i < skills_count; i++) {
        tlen = snprintf(text_buf, sizeof(text_buf), "%s %s",
                        skills[i].name ? skills[i].name : "",
                        skills[i].description ? skills[i].description : "");
        if (tlen < 0) tlen = 0;

        float *vec = NULL;
        size_t vdims = 0;
        err = embed_fn(embed_ctx, alloc, text_buf, (size_t)tlen, &vec, &vdims);
        if (err != HU_OK || vdims != dims) {
            if (vec) alloc->free(alloc->ctx, vec, vdims * sizeof(float));
            alloc->free(alloc->ctx, embeddings, total);
            return err != HU_OK ? err : HU_ERR_INVALID_ARGUMENT;
        }
        memcpy(embeddings + i * dims, vec, dims * sizeof(float));
        alloc->free(alloc->ctx, vec, dims * sizeof(float));
    }

    /* Clean up old context */
    hu_skill_routing_deinit(ctx, alloc);

    ctx->skill_embeddings = embeddings;
    ctx->skills_count = skills_count;
    ctx->embed_dims = dims;
    ctx->initialized = true;

    return HU_OK;
}

/* Comparison function for sorting routes by combined_score descending */
static int route_cmp(const void *a, const void *b) {
    const hu_skill_route_t *ra = (const hu_skill_route_t *)a;
    const hu_skill_route_t *rb = (const hu_skill_route_t *)b;
    if (rb->combined_score > ra->combined_score) return 1;
    if (rb->combined_score < ra->combined_score) return -1;
    return 0;
}

hu_error_t hu_skill_routing_route(const hu_skill_routing_ctx_t *ctx,
                                   hu_allocator_t *alloc,
                                   const char *user_msg, size_t user_msg_len,
                                   hu_embed_fn embed_fn, void *embed_ctx,
                                   const hu_skill_t *skills, size_t skills_count,
                                   const float *keyword_scores,
                                   const double *experience_weights,
                                   float emotional_intensity,
                                   hu_skill_blend_t *out) {
    if (!alloc || !skills || !out || skills_count == 0)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    /* Build route array */
    size_t routes_size = skills_count * sizeof(hu_skill_route_t);
    hu_skill_route_t *routes = alloc->alloc(alloc->ctx, routes_size);
    if (!routes) return HU_ERR_OUT_OF_MEMORY;

    /* Embed user message if semantic context available */
    float *msg_vec = NULL;
    size_t msg_dims = 0;
    bool have_semantic = false;

    if (ctx && ctx->initialized && embed_fn && user_msg && user_msg_len > 0) {
        hu_error_t err = embed_fn(embed_ctx, alloc, user_msg, user_msg_len,
                                   &msg_vec, &msg_dims);
        if (err == HU_OK && msg_dims == ctx->embed_dims)
            have_semantic = true;
        else if (msg_vec) {
            alloc->free(alloc->ctx, msg_vec, msg_dims * sizeof(float));
            msg_vec = NULL;
        }
    }

    for (size_t i = 0; i < skills_count; i++) {
        routes[i].skill = &skills[i];

        /* Semantic score */
        if (have_semantic) {
            routes[i].semantic_score = hu_cosine_similarity_f(
                msg_vec, ctx->skill_embeddings + i * ctx->embed_dims, ctx->embed_dims);
            if (routes[i].semantic_score < 0.0f) routes[i].semantic_score = 0.0f;
        } else {
            routes[i].semantic_score = 0.0f;
        }

        /* Keyword score */
        routes[i].keyword_score = keyword_scores ? keyword_scores[i] : 0.0f;

        /* Experience weight */
        routes[i].experience_weight = experience_weights
            ? (float)experience_weights[i] : 1.0f;

        /* Emotional boost: skills mentioning empathy/emotion get a lift */
        routes[i].emotional_boost = 0.0f;
        if (emotional_intensity > 0.3f && skills[i].description) {
            const char *desc = skills[i].description;
            if (strstr(desc, "emotion") || strstr(desc, "empathy") ||
                strstr(desc, "support") || strstr(desc, "conflict") ||
                strstr(desc, "listen")) {
                routes[i].emotional_boost = emotional_intensity * 0.2f;
            }
        }

        /* Combined score: weighted blend */
        float sem_w = have_semantic ? 0.5f : 0.0f;
        float key_w = have_semantic ? 0.3f : 1.0f;
        float exp_w = 0.15f;
        float emo_w = 0.05f;

        routes[i].combined_score =
            sem_w * routes[i].semantic_score +
            key_w * routes[i].keyword_score +
            exp_w * (routes[i].experience_weight - 1.0f) +
            emo_w * routes[i].emotional_boost;

        /* Scale by experience weight */
        if (routes[i].experience_weight != 1.0f) {
            routes[i].combined_score *= routes[i].experience_weight;
        }
    }

    if (msg_vec)
        alloc->free(alloc->ctx, msg_vec, msg_dims * sizeof(float));

    /* Sort by combined score descending */
    qsort(routes, skills_count, sizeof(hu_skill_route_t), route_cmp);

    /* Take top blend_max */
    out->count = skills_count < HU_SKILL_BLEND_MAX ? skills_count : HU_SKILL_BLEND_MAX;
    for (size_t i = 0; i < out->count; i++) {
        out->routes[i] = routes[i];
    }

    /* Drop secondaries if their score is much lower than primary */
    if (out->count > 1 && out->routes[0].combined_score > 0.01f) {
        float threshold = out->routes[0].combined_score * 0.4f;
        while (out->count > 1 &&
               out->routes[out->count - 1].combined_score < threshold) {
            out->count--;
        }
    }

    alloc->free(alloc->ctx, routes, routes_size);
    return HU_OK;
}

hu_error_t hu_skill_routing_build_catalog(hu_allocator_t *alloc,
                                           const hu_skill_blend_t *blend,
                                           const hu_skill_t *skills,
                                           size_t skills_count,
                                           size_t top_k,
                                           char **out, size_t *out_len) {
    (void)skills;
    (void)skills_count;
    (void)top_k;

    if (!alloc || !blend || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    if (blend->count == 0) return HU_OK;

    char buf[4096];
    int pos = 0;

    for (size_t i = 0; i < blend->count && i < HU_SKILL_BLEND_MAX; i++) {
        const hu_skill_route_t *r = &blend->routes[i];
        if (!r->skill || !r->skill->name) continue;

        if (i == 0) {
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                            "- **%s**: %s (relevance: %.0f%%)\n",
                            r->skill->name,
                            r->skill->description ? r->skill->description : "",
                            (double)(r->combined_score * 100.0f));
        } else {
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                            "- %s: %s\n",
                            r->skill->name,
                            r->skill->description ? r->skill->description : "");
        }
    }

    if (pos == 0) return HU_OK;

    size_t len = (size_t)pos;
    char *result = alloc->alloc(alloc->ctx, len + 1);
    if (!result) return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, buf, len);
    result[len] = '\0';
    *out = result;
    *out_len = len;
    return HU_OK;
}
