#include "human/memory/graph_index.h"
#include "human/core/string.h"
#include <ctype.h>
#include <string.h>

hu_error_t hu_graph_index_init(hu_graph_index_t *idx, hu_allocator_t *alloc) {
    if (!idx || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(idx, 0, sizeof(*idx));
    idx->alloc = alloc;
    idx->node_cap = 64;
    idx->nodes =
        (hu_graph_node_t *)alloc->alloc(alloc->ctx, idx->node_cap * sizeof(hu_graph_node_t));
    if (!idx->nodes)
        return HU_ERR_OUT_OF_MEMORY;
    memset(idx->nodes, 0, idx->node_cap * sizeof(hu_graph_node_t));
    return HU_OK;
}

void hu_graph_index_deinit(hu_graph_index_t *idx) {
    if (!idx || !idx->alloc)
        return;
    for (size_t i = 0; i < idx->node_count; i++) {
        if (idx->nodes[i].memory_key)
            idx->alloc->free(idx->alloc->ctx, idx->nodes[i].memory_key,
                             idx->nodes[i].memory_key_len + 1);
    }
    if (idx->nodes)
        idx->alloc->free(idx->alloc->ctx, idx->nodes, idx->node_cap * sizeof(hu_graph_node_t));
    memset(idx, 0, sizeof(*idx));
}

/* Lightweight entity extraction: capitalized words >2 chars. */
static size_t extract_entities(const char *content, size_t len, char out[][HU_GRAPH_MAX_ENTITY_LEN],
                               size_t max_entities) {
    size_t count = 0;
    size_t i = 0;
    while (i < len && count < max_entities) {
        while (i < len && !isalpha((unsigned char)content[i]))
            i++;
        if (i >= len)
            break;
        size_t start = i;
        while (i < len && (isalpha((unsigned char)content[i]) || content[i] == '\''))
            i++;
        size_t wlen = i - start;
        if (wlen < 3 || wlen >= HU_GRAPH_MAX_ENTITY_LEN)
            continue;
        if (!isupper((unsigned char)content[start]))
            continue;
        /* Skip if first word of sentence (after . or start) */
        if (start == 0)
            continue;
        if (start >= 2 && content[start - 2] == '.')
            continue;

        /* Deduplicate */
        bool dup = false;
        for (size_t j = 0; j < count; j++) {
            if (strlen(out[j]) == wlen && memcmp(out[j], content + start, wlen) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            memcpy(out[count], content + start, wlen);
            out[count][wlen] = '\0';
            count++;
        }
    }
    return count;
}

static int32_t find_node(const hu_graph_index_t *idx, const char *key, size_t key_len) {
    for (size_t i = 0; i < idx->node_count; i++) {
        if (idx->nodes[i].memory_key_len == key_len &&
            memcmp(idx->nodes[i].memory_key, key, key_len) == 0)
            return (int32_t)i;
    }
    return -1;
}

static void link_entity_edges(hu_graph_index_t *idx, size_t new_idx) {
    hu_graph_node_t *nn = &idx->nodes[new_idx];
    for (size_t e = 0; e < nn->entity_count; e++) {
        for (size_t i = 0; i < idx->node_count; i++) {
            if (i == new_idx)
                continue;
            hu_graph_node_t *other = &idx->nodes[i];
            for (size_t oe = 0; oe < other->entity_count; oe++) {
                if (strcmp(nn->entities[e], other->entities[oe]) == 0) {
                    if (nn->entity_edge_count < HU_GRAPH_MAX_ENTITIES_PER_ENTRY) {
                        nn->entity_edges[nn->entity_edge_count].target_idx = (uint32_t)i;
                        nn->entity_edges[nn->entity_edge_count].weight = 1.0f;
                        nn->entity_edge_count++;
                    }
                    break;
                }
            }
        }
    }
}

hu_error_t hu_graph_index_add(hu_graph_index_t *idx, const char *key, size_t key_len,
                              const char *content, size_t content_len, int64_t timestamp) {
    if (!idx || !key)
        return HU_ERR_INVALID_ARGUMENT;
    if (idx->node_count >= HU_GRAPH_MAX_ENTRIES)
        return HU_ERR_OUT_OF_MEMORY;

    if (idx->node_count >= idx->node_cap) {
        size_t new_cap = idx->node_cap * 2;
        if (new_cap > HU_GRAPH_MAX_ENTRIES)
            new_cap = HU_GRAPH_MAX_ENTRIES;
        hu_graph_node_t *nn = (hu_graph_node_t *)idx->alloc->realloc(
            idx->alloc->ctx, idx->nodes, idx->node_cap * sizeof(hu_graph_node_t),
            new_cap * sizeof(hu_graph_node_t));
        if (!nn)
            return HU_ERR_OUT_OF_MEMORY;
        memset(nn + idx->node_cap, 0, (new_cap - idx->node_cap) * sizeof(hu_graph_node_t));
        idx->nodes = nn;
        idx->node_cap = new_cap;
    }

    size_t ni = idx->node_count;
    hu_graph_node_t *node = &idx->nodes[ni];
    memset(node, 0, sizeof(*node));
    node->memory_key = hu_strndup(idx->alloc, key, key_len);
    node->memory_key_len = key_len;
    node->timestamp = timestamp;
    node->temporal_prev = (ni > 0) ? (int32_t)(ni - 1) : -1;
    node->temporal_next = -1;
    node->causal_next = -1;

    if (ni > 0)
        idx->nodes[ni - 1].temporal_next = (int32_t)ni;

    if (content && content_len > 0)
        node->entity_count =
            extract_entities(content, content_len, node->entities, HU_GRAPH_MAX_ENTITIES_PER_ENTRY);

    idx->node_count++;
    link_entity_edges(idx, ni);
    return HU_OK;
}

hu_error_t hu_graph_index_rerank(const hu_graph_index_t *idx, const char *query, size_t query_len,
                                 const char **keys, size_t *key_lens, double *scores,
                                 size_t count) {
    if (!idx || !keys || !scores)
        return HU_ERR_INVALID_ARGUMENT;
    (void)query;
    (void)query_len;

    for (size_t i = 0; i < count; i++) {
        int32_t ni = find_node(idx, keys[i], key_lens[i]);
        if (ni < 0)
            continue;
        const hu_graph_node_t *node = &idx->nodes[ni];

        /* Temporal boost: recent memories get a bump */
        double temporal_boost = 0.0;
        if (node->temporal_next == -1)
            temporal_boost = 0.05;

        /* Entity connectivity boost */
        double entity_boost = (double)node->entity_edge_count * 0.03;
        if (entity_boost > 0.15)
            entity_boost = 0.15;

        scores[i] += temporal_boost + entity_boost;
    }
    return HU_OK;
}

hu_error_t hu_graph_index_temporal_neighbors(const hu_graph_index_t *idx, const char *key,
                                             size_t key_len, const char **out_prev,
                                             const char **out_next) {
    if (!idx || !key)
        return HU_ERR_INVALID_ARGUMENT;
    if (out_prev)
        *out_prev = NULL;
    if (out_next)
        *out_next = NULL;

    int32_t ni = find_node(idx, key, key_len);
    if (ni < 0)
        return HU_OK;

    const hu_graph_node_t *node = &idx->nodes[ni];
    if (out_prev && node->temporal_prev >= 0 && (size_t)node->temporal_prev < idx->node_count)
        *out_prev = idx->nodes[node->temporal_prev].memory_key;
    if (out_next && node->temporal_next >= 0 && (size_t)node->temporal_next < idx->node_count)
        *out_next = idx->nodes[node->temporal_next].memory_key;
    return HU_OK;
}

hu_error_t hu_graph_index_entity_neighbors(const hu_graph_index_t *idx, const char *key,
                                           size_t key_len, const char **out_keys, size_t *out_count,
                                           size_t max_results) {
    if (!idx || !key || !out_keys || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;

    int32_t ni = find_node(idx, key, key_len);
    if (ni < 0)
        return HU_OK;

    const hu_graph_node_t *node = &idx->nodes[ni];
    for (size_t i = 0; i < node->entity_edge_count && *out_count < max_results; i++) {
        uint32_t ti = node->entity_edges[i].target_idx;
        if (ti < idx->node_count) {
            out_keys[*out_count] = idx->nodes[ti].memory_key;
            (*out_count)++;
        }
    }
    return HU_OK;
}

/* ── Spreading activation ─────────────────────────────────────────────── */

void hu_spread_activation_config_default(hu_spread_activation_config_t *cfg) {
    if (!cfg)
        return;
    cfg->initial_energy = 1.0;
    cfg->decay_factor = 0.5;
    cfg->threshold = 0.05;
    cfg->max_hops = 3;
    cfg->max_activated = 16;
}

hu_error_t hu_graph_index_spread_activation(const hu_graph_index_t *idx,
                                            const hu_spread_activation_config_t *cfg,
                                            const uint32_t *seed_indices, size_t seed_count,
                                            hu_activated_node_t *out_nodes, size_t *out_count) {
    if (!idx || !cfg || !out_nodes || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    if (seed_count == 0 || idx->node_count == 0)
        return HU_OK;

    size_t nc = idx->node_count;
    if (nc > HU_GRAPH_MAX_ENTRIES)
        nc = HU_GRAPH_MAX_ENTRIES;

    double energy[HU_GRAPH_MAX_ENTRIES];
    memset(energy, 0, nc * sizeof(double));

    for (size_t i = 0; i < seed_count; i++) {
        if (seed_indices[i] < nc)
            energy[seed_indices[i]] = cfg->initial_energy;
    }

    for (uint32_t hop = 0; hop < cfg->max_hops; hop++) {
        double next[HU_GRAPH_MAX_ENTRIES];
        memset(next, 0, nc * sizeof(double));

        for (size_t n = 0; n < nc; n++) {
            if (energy[n] < cfg->threshold)
                continue;
            double spread = energy[n] * cfg->decay_factor;
            const hu_graph_node_t *node = &idx->nodes[n];

            /* Spread through entity edges */
            for (size_t e = 0; e < node->entity_edge_count; e++) {
                uint32_t ti = node->entity_edges[e].target_idx;
                if (ti < nc)
                    next[ti] += spread * (double)node->entity_edges[e].weight;
            }

            /* Spread through temporal edges */
            if (node->temporal_prev >= 0 && (size_t)node->temporal_prev < nc)
                next[node->temporal_prev] += spread * 0.7;
            if (node->temporal_next >= 0 && (size_t)node->temporal_next < nc)
                next[node->temporal_next] += spread * 0.7;

            /* Spread through causal edges */
            if (node->causal_next >= 0 && (size_t)node->causal_next < nc)
                next[node->causal_next] += spread * 0.9;
        }

        for (size_t n = 0; n < nc; n++) {
            energy[n] += next[n];
            if (energy[n] > cfg->initial_energy * 2.0)
                energy[n] = cfg->initial_energy * 2.0;
        }
    }

    /* Collect activated nodes above threshold, excluding seeds */
    hu_activated_node_t candidates[HU_GRAPH_MAX_ENTRIES];
    size_t cand_count = 0;
    for (size_t n = 0; n < nc; n++) {
        if (energy[n] < cfg->threshold)
            continue;
        bool is_seed = false;
        for (size_t s = 0; s < seed_count; s++) {
            if (seed_indices[s] == (uint32_t)n) {
                is_seed = true;
                break;
            }
        }
        if (is_seed)
            continue;
        candidates[cand_count].node_idx = (uint32_t)n;
        candidates[cand_count].energy = energy[n];
        cand_count++;
    }

    /* Sort by descending energy (simple insertion sort — bounded by MAX_ENTRIES) */
    for (size_t i = 1; i < cand_count; i++) {
        hu_activated_node_t tmp = candidates[i];
        size_t j = i;
        while (j > 0 && candidates[j - 1].energy < tmp.energy) {
            candidates[j] = candidates[j - 1];
            j--;
        }
        candidates[j] = tmp;
    }

    size_t out_cap = cfg->max_activated < cand_count ? cfg->max_activated : cand_count;
    for (size_t i = 0; i < out_cap; i++)
        out_nodes[i] = candidates[i];
    *out_count = out_cap;
    return HU_OK;
}
