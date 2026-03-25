#include "human/memory/graph_index.h"
#include "human/core/string.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

/* ── Hierarchical topic clusters ────────────────────────────────────── */

hu_error_t hu_graph_hierarchy_init(hu_graph_hierarchy_t *h, hu_allocator_t *alloc) {
    if (!h || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(h, 0, sizeof(*h));
    h->alloc = alloc;
    h->cluster_cap = 16;
    h->clusters =
        (hu_topic_cluster_t *)alloc->alloc(alloc->ctx, h->cluster_cap * sizeof(hu_topic_cluster_t));
    if (!h->clusters)
        return HU_ERR_OUT_OF_MEMORY;
    memset(h->clusters, 0, h->cluster_cap * sizeof(hu_topic_cluster_t));
    return HU_OK;
}

void hu_graph_hierarchy_deinit(hu_graph_hierarchy_t *h) {
    if (!h || !h->alloc)
        return;
    for (size_t c = 0; c < h->cluster_count; c++) {
        if (h->clusters[c].member_indices)
            h->alloc->free(h->alloc->ctx, h->clusters[c].member_indices,
                           h->clusters[c].member_cap * sizeof(uint32_t));
    }
    if (h->clusters)
        h->alloc->free(h->alloc->ctx, h->clusters, h->cluster_cap * sizeof(hu_topic_cluster_t));
    memset(h, 0, sizeof(*h));
}

static size_t entity_global_freq(const hu_graph_index_t *idx, const char *ent) {
    size_t n = 0;
    for (size_t i = 0; i < idx->node_count; i++) {
        const hu_graph_node_t *node = &idx->nodes[i];
        for (size_t e = 0; e < node->entity_count; e++) {
            if (strcmp(node->entities[e], ent) == 0) {
                n++;
                break;
            }
        }
    }
    return n;
}

static hu_error_t cluster_add_member(hu_graph_hierarchy_t *h, size_t cluster_idx, uint32_t node_i) {
    hu_topic_cluster_t *cl = &h->clusters[cluster_idx];
    if (cl->member_count >= cl->member_cap) {
        size_t ncap = cl->member_cap ? cl->member_cap * 2 : 8;
        uint32_t *nm = (uint32_t *)h->alloc->realloc(h->alloc->ctx, cl->member_indices,
                                                     cl->member_cap * sizeof(uint32_t),
                                                     ncap * sizeof(uint32_t));
        if (!nm)
            return HU_ERR_OUT_OF_MEMORY;
        cl->member_indices = nm;
        cl->member_cap = ncap;
    }
    for (size_t j = 0; j < cl->member_count; j++) {
        if (cl->member_indices[j] == node_i)
            return HU_OK;
    }
    cl->member_indices[cl->member_count++] = node_i;
    return HU_OK;
}

static size_t find_or_add_cluster(hu_graph_hierarchy_t *h, const char *label) {
    for (size_t c = 0; c < h->cluster_count; c++) {
        if (strcmp(h->clusters[c].label, label) == 0)
            return c;
    }
    if (h->cluster_count >= h->cluster_cap) {
        size_t ncap = h->cluster_cap * 2;
        hu_topic_cluster_t *nc = (hu_topic_cluster_t *)h->alloc->realloc(
            h->alloc->ctx, h->clusters, h->cluster_cap * sizeof(hu_topic_cluster_t),
            ncap * sizeof(hu_topic_cluster_t));
        if (!nc)
            return (size_t)-1;
        memset(nc + h->cluster_cap, 0, (ncap - h->cluster_cap) * sizeof(hu_topic_cluster_t));
        h->clusters = nc;
        h->cluster_cap = ncap;
    }
    size_t idx = h->cluster_count++;
    memset(&h->clusters[idx], 0, sizeof(h->clusters[idx]));
    (void)snprintf(h->clusters[idx].label, sizeof(h->clusters[idx].label), "%s", label);
    h->clusters[idx].centroid_score = 1.0;
    return idx;
}

hu_error_t hu_graph_hierarchy_build(hu_graph_hierarchy_t *h, const hu_graph_index_t *idx) {
    if (!h || !h->alloc || !idx)
        return HU_ERR_INVALID_ARGUMENT;

    size_t prev_clusters = h->cluster_count;
    for (size_t c = 0; c < prev_clusters; c++) {
        if (h->clusters[c].member_indices)
            h->alloc->free(h->alloc->ctx, h->clusters[c].member_indices,
                           h->clusters[c].member_cap * sizeof(uint32_t));
        memset(&h->clusters[c], 0, sizeof(h->clusters[c]));
    }
    h->cluster_count = 0;

    if (idx->node_count == 0)
        return HU_OK;

    for (size_t ni = 0; ni < idx->node_count; ni++) {
        const hu_graph_node_t *node = &idx->nodes[ni];
        const char *chosen = "(misc)";
        size_t best_freq = 0;

        if (node->entity_count > 0) {
            for (size_t e = 0; e < node->entity_count; e++) {
                size_t fq = entity_global_freq(idx, node->entities[e]);
                if (fq > best_freq) {
                    best_freq = fq;
                    chosen = node->entities[e];
                }
            }
        }

        size_t ci = find_or_add_cluster(h, chosen);
        if (ci == (size_t)-1)
            return HU_ERR_OUT_OF_MEMORY;
        hu_error_t err = cluster_add_member(h, ci, (uint32_t)ni);
        if (err != HU_OK)
            return err;
    }

    return HU_OK;
}

static size_t query_term_overlap(const char *qlabel, size_t qlen, const char *clabel) {
    char qbuf[256];
    char cbuf[256];
    if (qlen >= sizeof(qbuf))
        qlen = sizeof(qbuf) - 1;
    memcpy(qbuf, qlabel, qlen);
    qbuf[qlen] = '\0';
    (void)snprintf(cbuf, sizeof(cbuf), "%s", clabel);

    size_t score = 0;
    char *saveq = NULL;
    for (char *tok = strtok_r(qbuf, " \t\n\r", &saveq); tok;
         tok = strtok_r(NULL, " \t\n\r", &saveq)) {
        if (strlen(tok) < 2)
            continue;
        if (strstr(cbuf, tok) != NULL)
            score++;
    }
    return score;
}

hu_error_t hu_graph_hierarchy_traverse(const hu_graph_hierarchy_t *h, const hu_graph_index_t *idx,
                                       const char *query, size_t query_len, uint32_t *out_indices,
                                       size_t *out_count, size_t max_results) {
    if (!out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    if (!h || !idx || !out_indices || max_results == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (h->cluster_count == 0 || !query || query_len == 0)
        return HU_OK;

    typedef struct {
        size_t idx;
        size_t score;
    } ranked_t;

    ranked_t ranked[128];
    size_t rn = 0;
    for (size_t c = 0; c < h->cluster_count && rn < 128; c++) {
        size_t ov = query_term_overlap(query, query_len, h->clusters[c].label);
        if (ov == 0 && strcmp(h->clusters[c].label, "(misc)") != 0)
            continue;
        ranked[rn].idx = c;
        ranked[rn].score = ov > 0 ? ov : 1;
        rn++;
    }

    for (size_t i = 1; i < rn; i++) {
        ranked_t t = ranked[i];
        size_t j = i;
        while (j > 0 && ranked[j - 1].score < t.score) {
            ranked[j] = ranked[j - 1];
            j--;
        }
        ranked[j] = t;
    }

    if (rn == 0) {
        for (size_t c = 0; c < h->cluster_count && rn < 128; c++) {
            if (strcmp(h->clusters[c].label, "(misc)") == 0) {
                ranked[0].idx = c;
                ranked[0].score = 1;
                rn = 1;
                break;
            }
        }
    }

    for (size_t r = 0; r < rn && *out_count < max_results; r++) {
        const hu_topic_cluster_t *cl = &h->clusters[ranked[r].idx];
        for (size_t m = 0; m < cl->member_count && *out_count < max_results; m++) {
            uint32_t ni = cl->member_indices[m];
            if (ni >= idx->node_count)
                continue;
            bool dup = false;
            for (size_t z = 0; z < *out_count; z++) {
                if (out_indices[z] == ni) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                out_indices[(*out_count)++] = ni;
        }
    }

    return HU_OK;
}
