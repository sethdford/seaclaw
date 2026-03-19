/*
 * Tree-of-Thought — explores multiple reasoning branches for complex problems.
 * Generates N candidate thought paths, evaluates each, and selects the best.
 */

#include "human/agent/tree_of_thought.h"
#include "human/core/string.h"
#include "human/provider.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOT_GEN_PREFIX "THOUGHT "
#define TOT_GEN_PREFIX_LEN 7

#if defined(__GNUC__) || defined(__clang__)
#define TOT_UNUSED __attribute__((unused))
#else
#define TOT_UNUSED
#endif

static TOT_UNUSED const char TOT_GEN_SYS[] =
    "You are a reasoning assistant. Given a problem, produce exactly %d different "
    "reasoning approaches. Respond with a JSON object: {\"thoughts\":[\"approach 1\",\"approach 2\",...]}. "
    "No other text outside the JSON.";

static TOT_UNUSED const char TOT_EVAL_SYS[] =
    "Rate how promising this reasoning path is for solving the problem, from 0.0 to 1.0. "
    "Reply with ONLY a number (e.g. 0.85). No explanation.";

static TOT_UNUSED const char TOT_EXPAND_SYS[] =
    "Given this reasoning approach: %s, generate exactly %d more specific sub-approaches. "
    "Respond with a JSON object: {\"thoughts\":[\"sub-approach 1\",\"sub-approach 2\",...]}. "
    "No other text outside the JSON.";

hu_tot_config_t hu_tot_config_default(void) {
    return (hu_tot_config_t){
        .num_branches = 3,
        .max_depth = 2,
        .prune_threshold = 0.3,
        .enabled = true,
        .beam_width = 3,
        .max_total_nodes = 50,
        .strategy = HU_TOT_STRATEGY_BEAM_SEARCH,
    };
}

void hu_tot_result_free(hu_allocator_t *alloc, hu_tot_result_t *result) {
    if (!alloc || !result)
        return;
    if (result->best_thought) {
        hu_str_free(alloc, result->best_thought);
        result->best_thought = NULL;
    }
    result->best_thought_len = 0;
    result->best_score = 0.0;
    result->branches_explored = 0;
    result->branches_pruned = 0;
    result->max_depth_reached = 0;
    result->llm_calls_made = 0;
}

/* Parse JSON {"thoughts":["...","..."]} from response. Returns count parsed, 0 on failure. */
static TOT_UNUSED size_t parse_thoughts_json(hu_allocator_t *alloc, const char *resp, size_t resp_len,
                                  hu_tot_branch_t *branches, size_t max_branches, int depth_hint) {
    if (!resp || resp_len == 0 || !branches || max_branches == 0)
        return 0;

    const char *arr = NULL;
    for (size_t i = 0; i + 11 < resp_len; i++) {
        if (memcmp(resp + i, "\"thoughts\"", 10) == 0) {
            const char *p = resp + i + 10;
            while (p < resp + resp_len && (*p == ' ' || *p == ':' || *p == '\t'))
                p++;
            if (p < resp + resp_len && *p == '[') {
                arr = p + 1;
                break;
            }
        }
    }
    if (!arr) return 0;

    size_t count = 0;
    const char *end = resp + resp_len;
    const char *p = arr;

    while (p < end && count < max_branches) {
        while (p < end && *p != '"' && *p != ']') p++;
        if (p >= end || *p == ']') break;
        p++; /* skip opening quote */

        const char *start = p;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) p++; /* skip escaped chars */
            p++;
        }
        size_t len = (size_t)(p - start);
        if (len > 0) {
            char *thought = hu_strndup(alloc, start, len);
            if (thought) {
                branches[count].thought = thought;
                branches[count].thought_len = strlen(thought);
                branches[count].score = 0.0;
                branches[count].depth = depth_hint;
                count++;
            }
        }
        if (p < end) p++; /* skip closing quote */
    }
    return count;
}

/* Parse "THOUGHT N: content" lines (legacy fallback). Returns count of branches parsed.
 * depth_hint: depth to assign to parsed branches (0 for root, 1 for expansion). */
static TOT_UNUSED size_t parse_thoughts_text(hu_allocator_t *alloc, const char *resp, size_t resp_len,
                              hu_tot_branch_t *branches, size_t max_branches, int depth_hint) {
    if (!resp || resp_len == 0 || !branches || max_branches == 0)
        return 0;

    size_t count = 0;
    const char *p = resp;
    const char *end = resp + resp_len;

    while (p < end && count < max_branches) {
        /* Skip leading whitespace and newlines */
        while (p < end && (isspace((unsigned char)*p) || *p == '\n'))
            p++;
        if (p >= end)
            break;

        /* Match "THOUGHT N:" */
        if (p + TOT_GEN_PREFIX_LEN >= end || memcmp(p, TOT_GEN_PREFIX, TOT_GEN_PREFIX_LEN) != 0) {
            /* Skip to next line */
            while (p < end && *p != '\n')
                p++;
            continue;
        }
        p += TOT_GEN_PREFIX_LEN;

        /* Skip digit(s) and colon */
        while (p < end && (*p >= '0' && *p <= '9'))
            p++;
        if (p < end && *p == ':')
            p++;
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;

        const char *start = p;
        while (p < end && *p != '\n')
            p++;

        size_t len = (size_t)(p - start);
        if (len > 0) {
            char *thought = hu_strndup(alloc, start, len);
            if (thought) {
                branches[count].thought = thought;
                branches[count].thought_len = strlen(thought);
                branches[count].score = 0.0;
                branches[count].depth = depth_hint;
                count++;
            }
        }
    }
    return count;
}

/* Unified parser: tries JSON first, falls back to text format for robustness. */
static TOT_UNUSED size_t parse_thoughts(hu_allocator_t *alloc, const char *resp, size_t resp_len,
                             hu_tot_branch_t *branches, size_t max_branches, int depth_hint) {
    size_t count = parse_thoughts_json(alloc, resp, resp_len, branches, max_branches, depth_hint);
    if (count > 0) return count;
    return parse_thoughts_text(alloc, resp, resp_len, branches, max_branches, depth_hint);
}

/* Parse a single float 0.0-1.0 from response. Returns -1.0 on parse failure. */
static TOT_UNUSED double parse_score(const char *resp, size_t resp_len) {
    if (!resp || resp_len == 0)
        return -1.0;
    const char *p = resp;
    while (*p && (isspace((unsigned char)*p) || *p == '\n'))
        p++;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p || v < 0.0 || v > 1.0)
        return -1.0;
    return v;
}

hu_error_t hu_tot_explore(hu_allocator_t *alloc, hu_provider_t *provider,
                          const char *model, size_t model_len,
                          const char *problem, size_t problem_len,
                          const hu_tot_config_t *config,
                          hu_tot_result_t *result) {
    if (!alloc || !result)
        return HU_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

    int num_branches = config ? config->num_branches : 3;
    double prune_threshold = config ? config->prune_threshold : 0.3;
    if (num_branches <= 0 || num_branches > HU_TOT_MAX_BRANCHES)
        num_branches = 3;

#ifdef HU_IS_TEST
    (void)provider;
    (void)model;
    (void)model_len;
    (void)problem;
    (void)problem_len;

    /* Recursive mock: 3 root branches, 2 children per surviving, 1 grandchild per surviving. */
    static const char *mock_root[] = {"Break into subproblems", "Use analogy", "Work backwards"};
    static const double mock_root_scores[] = {0.9, 0.6, 0.25};

    int max_depth = config ? config->max_depth : 2;
    int beam_width = config && config->beam_width > 0 ? config->beam_width : 3;
    int max_total_nodes = config && config->max_total_nodes > 0 ? config->max_total_nodes : 50;

    size_t pruned = 0;
    size_t total_explored = 0;
    int max_depth_reached = 0;
    const char *best_thought_str = NULL;
    double best_score = -1.0;

    /* Level 0: 3 root branches */
    for (int i = 0; i < 3 && total_explored < (size_t)max_total_nodes; i++) {
        total_explored++;
        if (mock_root_scores[i] < prune_threshold)
            pruned++;
        else if (mock_root_scores[i] > best_score) {
            best_score = mock_root_scores[i];
            best_thought_str = mock_root[i];
        }
    }
    /* Surviving roots (score > prune): 0.9, 0.6. Keep top beam_width. */
    double level0_scores[3] = {0.9, 0.6, 0.25};
    /* Sort by score desc, take top beam_width */
    int survivors0[3];
    int n0 = 0;
    for (int i = 0; i < 3 && n0 < beam_width; i++) {
        int best_idx = -1;
        double best_s = -1.0;
        for (int j = 0; j < 3; j++) {
            int used = 0;
            for (int k = 0; k < n0; k++)
                if (survivors0[k] == j)
                    used = 1;
            if (!used && level0_scores[j] >= prune_threshold && level0_scores[j] > best_s) {
                best_s = level0_scores[j];
                best_idx = j;
            }
        }
        if (best_idx < 0)
            break;
        survivors0[n0++] = best_idx;
    }

    /* Level 1: 2 children per surviving root, scores = parent ± 0.05 */
    if (max_depth > 1 && total_explored < (size_t)max_total_nodes) {
        max_depth_reached = 1;
        for (int s = 0; s < n0 && total_explored < (size_t)max_total_nodes; s++) {
            int p = survivors0[s];
            double ps = level0_scores[p];
            int is_sub = (p == 0);
            for (int c = 0; c < 2 && total_explored < (size_t)max_total_nodes; c++) {
                double cs = ps + (c == 0 ? 0.05 : -0.05);
                total_explored++;
                if (cs < prune_threshold)
                    pruned++;
                else if (cs > best_score) {
                    best_score = cs;
                    best_thought_str = is_sub ? "Break into subproblems (refined)" : "Use analogy (refined)";
                }
            }
        }

        /* Level 2: 1 grandchild per surviving at level 1, keep top beam_width at level 1 first */
        double l1_scores[6];
        const char *l1_thoughts[6];
        int l1_count = 0;
        for (int s = 0; s < n0; s++) {
            int p = survivors0[s];
            double ps = level0_scores[p];
            int is_sub = (p == 0);
            for (int c = 0; c < 2; c++) {
                double cs = ps + (c == 0 ? 0.05 : -0.05);
                if (cs >= prune_threshold) {
                    l1_scores[l1_count] = cs;
                    l1_thoughts[l1_count] = is_sub ? "Break into subproblems (refined)" : "Use analogy (refined)";
                    l1_count++;
                }
            }
        }
        /* Sort l1 by score, take top beam_width */
        int surv1[6];
        int n1 = 0;
        for (int i = 0; i < l1_count && n1 < beam_width; i++) {
            int best_idx = -1;
            double best_s = -1.0;
            for (int j = 0; j < l1_count; j++) {
                int used = 0;
                for (int k = 0; k < n1; k++)
                    if (surv1[k] == j)
                        used = 1;
                if (!used && l1_scores[j] > best_s) {
                    best_s = l1_scores[j];
                    best_idx = j;
                }
            }
            if (best_idx < 0)
                break;
            surv1[n1++] = best_idx;
        }

        if (max_depth > 2 && total_explored < (size_t)max_total_nodes) {
            max_depth_reached = 2;
            for (int s = 0; s < n1 && total_explored < (size_t)max_total_nodes; s++) {
                double ps = l1_scores[surv1[s]];
                int is_sub = (l1_thoughts[surv1[s]] != NULL && strstr(l1_thoughts[surv1[s]], "subproblems") != NULL);
                double gs = ps + 0.05;
                if (gs > 1.0)
                    gs = 1.0;
                total_explored++;
                if (gs < prune_threshold)
                    pruned++;
                else if (gs > best_score) {
                    best_score = gs;
                    best_thought_str = is_sub ? "Break into subproblems (refined further)" : "Use analogy (refined further)";
                }
            }
        }
    }

    if (best_thought_str) {
        result->best_thought = hu_strdup(alloc, best_thought_str);
        if (result->best_thought) {
            result->best_thought_len = strlen(result->best_thought);
            result->best_score = best_score;
        }
    }
    result->branches_explored = total_explored;
    result->branches_pruned = pruned;
    result->max_depth_reached = max_depth_reached;
    result->llm_calls_made = 0;
    return HU_OK;
#else
    if (!provider || !provider->vtable || !provider->vtable->chat_with_system)
        return HU_ERR_NOT_SUPPORTED;
    if (!problem || problem_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    int max_depth = config ? config->max_depth : 2;
    int beam_width = config && config->beam_width > 0 ? config->beam_width : 3;
    int node_limit = config && config->max_total_nodes > 0 ? config->max_total_nodes : 50;
    if (max_depth > 5) max_depth = 5;
    if (beam_width > HU_TOT_MAX_BRANCHES) beam_width = HU_TOT_MAX_BRANCHES;

    const char *gen_model = (model && model_len > 0) ? model : "gpt-4o-mini";
    size_t gen_model_len = (model && model_len > 0) ? model_len : 11;
    const char *eval_sys = TOT_EVAL_SYS;
    size_t eval_sys_len = sizeof(TOT_EVAL_SYS) - 1;

    size_t pruned = 0;
    size_t total_explored = 0;
    int llm_calls = 0;
    int max_depth_reached = 0;
    double best_score = -1.0;
    char *best_thought = NULL;

    /* Beam frontier: the top-K branches surviving from the previous level */
    hu_tot_branch_t beam[HU_TOT_MAX_BRANCHES];
    int beam_count = 0;

    /* Level 0: generate root thoughts */
    {
        char *gen_sys_str = hu_sprintf(alloc, TOT_GEN_SYS, num_branches);
        if (!gen_sys_str) return HU_ERR_OUT_OF_MEMORY;

        char *gen_out = NULL;
        size_t gen_out_len = 0;
        hu_error_t err = provider->vtable->chat_with_system(
            provider->ctx, alloc, gen_sys_str, strlen(gen_sys_str), problem, problem_len,
            gen_model, gen_model_len, 0.7, &gen_out, &gen_out_len);
        hu_str_free(alloc, gen_sys_str);
        llm_calls++;

        if (err != HU_OK || !gen_out || gen_out_len == 0) {
            if (gen_out) alloc->free(alloc->ctx, gen_out, gen_out_len + 1);
            return gen_out ? HU_ERR_PROVIDER_RESPONSE : err;
        }

        hu_tot_branch_t roots[HU_TOT_MAX_BRANCHES];
        memset(roots, 0, sizeof(roots));
        size_t root_count = parse_thoughts(alloc, gen_out, gen_out_len, roots,
                                            HU_TOT_MAX_BRANCHES, 0);
        alloc->free(alloc->ctx, gen_out, gen_out_len + 1);

        /* Score each root */
        for (size_t i = 0; i < root_count && total_explored < (size_t)node_limit; i++) {
            total_explored++;
            char *user_msg = hu_sprintf(alloc, "Rate this approach on a scale of 0.0 to 1.0: %s",
                                         roots[i].thought);
            if (!user_msg) continue;
            char *eval_out = NULL;
            size_t eval_out_len = 0;
            err = provider->vtable->chat_with_system(
                provider->ctx, alloc, eval_sys, eval_sys_len, user_msg, strlen(user_msg),
                gen_model, gen_model_len, 0.0, &eval_out, &eval_out_len);
            alloc->free(alloc->ctx, user_msg, strlen(user_msg) + 1);
            llm_calls++;
            if (err == HU_OK && eval_out && eval_out_len > 0) {
                double s = parse_score(eval_out, eval_out_len);
                if (s >= 0.0) roots[i].score = s;
            }
            if (eval_out) alloc->free(alloc->ctx, eval_out, eval_out_len + 1);

            if (roots[i].score < prune_threshold)
                pruned++;
            else if (roots[i].score > best_score) {
                best_score = roots[i].score;
                if (best_thought) hu_str_free(alloc, best_thought);
                best_thought = hu_strdup(alloc, roots[i].thought);
            }
        }

        /* Select top beam_width roots above threshold */
        beam_count = 0;
        memset(beam, 0, sizeof(beam));
        for (int b = 0; b < beam_width && b < (int)root_count; b++) {
            int best_idx = -1;
            double best_s = -1.0;
            for (size_t j = 0; j < root_count; j++) {
                if (roots[j].score < prune_threshold) continue;
                bool used = false;
                for (int k = 0; k < beam_count; k++)
                    if (beam[k].thought && roots[j].thought &&
                        strcmp(beam[k].thought, roots[j].thought) == 0)
                        used = true;
                if (!used && roots[j].score > best_s) {
                    best_s = roots[j].score;
                    best_idx = (int)j;
                }
            }
            if (best_idx < 0) break;
            beam[beam_count].thought = hu_strdup(alloc, roots[best_idx].thought);
            beam[beam_count].thought_len = roots[best_idx].thought_len;
            beam[beam_count].score = roots[best_idx].score;
            beam[beam_count].depth = 0;
            beam_count++;
        }

        for (size_t j = 0; j < root_count; j++)
            if (roots[j].thought) hu_str_free(alloc, roots[j].thought);
    }

    /* Deeper levels: expand each beam member, collect children, select top-K */
    for (int depth = 1; depth < max_depth && beam_count > 0 &&
         total_explored < (size_t)node_limit; depth++) {
        max_depth_reached = depth;

        hu_tot_branch_t candidates[HU_TOT_MAX_BRANCHES * HU_TOT_MAX_BRANCHES];
        memset(candidates, 0, sizeof(candidates));
        int cand_count = 0;

        for (int b = 0; b < beam_count && total_explored < (size_t)node_limit; b++) {
            char *expand_sys = hu_sprintf(alloc, TOT_EXPAND_SYS, beam[b].thought,
                                           num_branches);
            if (!expand_sys) continue;

            char *expand_out = NULL;
            size_t expand_out_len = 0;
            hu_error_t err = provider->vtable->chat_with_system(
                provider->ctx, alloc, expand_sys, strlen(expand_sys), problem, problem_len,
                gen_model, gen_model_len, 0.7, &expand_out, &expand_out_len);
            hu_str_free(alloc, expand_sys);
            llm_calls++;

            if (err != HU_OK || !expand_out || expand_out_len == 0) {
                if (expand_out) alloc->free(alloc->ctx, expand_out, expand_out_len + 1);
                continue;
            }

            hu_tot_branch_t children[HU_TOT_MAX_BRANCHES];
            memset(children, 0, sizeof(children));
            size_t child_count = parse_thoughts(alloc, expand_out, expand_out_len,
                                                 children, HU_TOT_MAX_BRANCHES, depth);
            alloc->free(alloc->ctx, expand_out, expand_out_len + 1);

            for (size_t c = 0; c < child_count && total_explored < (size_t)node_limit; c++) {
                total_explored++;
                char *user_msg = hu_sprintf(alloc,
                    "Rate this approach on a scale of 0.0 to 1.0: %s", children[c].thought);
                if (!user_msg) continue;
                char *eval_out = NULL;
                size_t eval_out_len = 0;
                err = provider->vtable->chat_with_system(
                    provider->ctx, alloc, eval_sys, eval_sys_len, user_msg, strlen(user_msg),
                    gen_model, gen_model_len, 0.0, &eval_out, &eval_out_len);
                alloc->free(alloc->ctx, user_msg, strlen(user_msg) + 1);
                llm_calls++;
                if (err == HU_OK && eval_out && eval_out_len > 0) {
                    double s = parse_score(eval_out, eval_out_len);
                    if (s >= 0.0) children[c].score = s;
                }
                if (eval_out) alloc->free(alloc->ctx, eval_out, eval_out_len + 1);

                if (children[c].score < prune_threshold) {
                    pruned++;
                } else {
                    if (children[c].score > best_score) {
                        best_score = children[c].score;
                        if (best_thought) hu_str_free(alloc, best_thought);
                        best_thought = hu_strdup(alloc, children[c].thought);
                    }
                    if (cand_count < (int)(sizeof(candidates) / sizeof(candidates[0]))) {
                        candidates[cand_count].thought = hu_strdup(alloc, children[c].thought);
                        candidates[cand_count].thought_len = children[c].thought_len;
                        candidates[cand_count].score = children[c].score;
                        candidates[cand_count].depth = depth;
                        cand_count++;
                    }
                }
            }

            for (size_t j = 0; j < child_count; j++)
                if (children[j].thought) hu_str_free(alloc, children[j].thought);
        }

        /* Free old beam */
        for (int b = 0; b < beam_count; b++)
            if (beam[b].thought) hu_str_free(alloc, beam[b].thought);

        /* Select top beam_width from candidates */
        beam_count = 0;
        memset(beam, 0, sizeof(beam));
        for (int bw = 0; bw < beam_width && bw < cand_count; bw++) {
            int best_idx = -1;
            double best_s = -1.0;
            for (int j = 0; j < cand_count; j++) {
                bool used = false;
                for (int k = 0; k < beam_count; k++)
                    if (beam[k].thought && candidates[j].thought &&
                        strcmp(beam[k].thought, candidates[j].thought) == 0)
                        used = true;
                if (!used && candidates[j].score > best_s) {
                    best_s = candidates[j].score;
                    best_idx = j;
                }
            }
            if (best_idx < 0) break;
            beam[beam_count].thought = hu_strdup(alloc, candidates[best_idx].thought);
            beam[beam_count].thought_len = candidates[best_idx].thought_len;
            beam[beam_count].score = candidates[best_idx].score;
            beam[beam_count].depth = candidates[best_idx].depth;
            beam_count++;
        }

        for (int j = 0; j < cand_count; j++)
            if (candidates[j].thought) hu_str_free(alloc, candidates[j].thought);
    }

    /* Cleanup beam */
    for (int b = 0; b < beam_count; b++)
        if (beam[b].thought) hu_str_free(alloc, beam[b].thought);

    result->branches_explored = total_explored;
    result->branches_pruned = pruned;
    result->max_depth_reached = max_depth_reached;
    result->llm_calls_made = llm_calls;

    if (best_thought) {
        result->best_thought = best_thought;
        result->best_thought_len = strlen(best_thought);
        result->best_score = best_score;
    }

    return HU_OK;
#endif
}
