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
#define HU_TOT_MAX_TOTAL_NODES 50

#if defined(__GNUC__) || defined(__clang__)
#define TOT_UNUSED __attribute__((unused))
#else
#define TOT_UNUSED
#endif

static TOT_UNUSED const char TOT_GEN_SYS[] =
    "You are a reasoning assistant. Given a problem, produce exactly %d different "
    "reasoning approaches. Each approach must be on its own line, prefixed with "
    "\"THOUGHT N:\" where N is 1, 2, 3, etc. Be concise. No other text.";

static TOT_UNUSED const char TOT_EVAL_SYS[] =
    "Rate how promising this reasoning path is for solving the problem, from 0.0 to 1.0. "
    "Reply with ONLY a number (e.g. 0.85). No explanation.";

static TOT_UNUSED const char TOT_EXPAND_SYS[] =
    "Given this reasoning approach: %s, generate exactly %d more specific sub-approaches. "
    "Each on its own line, prefixed with \"THOUGHT N:\" where N is 1, 2, 3, etc. Be concise.";

hu_tot_config_t hu_tot_config_default(void) {
    return (hu_tot_config_t){
        .num_branches = 3,
        .max_depth = 2,
        .prune_threshold = 0.3,
        .enabled = true,
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
}

/* Parse "THOUGHT N: content" lines from response. Returns count of branches parsed.
 * depth_hint: depth to assign to parsed branches (0 for root, 1 for expansion). */
static TOT_UNUSED size_t parse_thoughts(hu_allocator_t *alloc, const char *resp, size_t resp_len,
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

    static const char *mock_thoughts[] = {"Break into subproblems", "Use analogy", "Work backwards"};
    static const double mock_scores[] = {0.9, 0.6, 0.25};

    int max_depth = config ? config->max_depth : 2;
    size_t n = (size_t)num_branches;
    if (n > 3)
        n = 3;

    size_t pruned = 0;
    size_t total_explored = n;
    const char *best_thought_str = NULL;
    double best_score = -1.0;

    for (size_t i = 0; i < n; i++) {
        if (mock_scores[i] < prune_threshold)
            pruned++;
        else if (mock_scores[i] > best_score) {
            best_score = mock_scores[i];
            best_thought_str = mock_thoughts[i];
        }
    }

    if (max_depth > 1) {
        best_score = 0.95;
        best_thought_str = "Break into subproblems (refined)";
        total_explored += (size_t)(max_depth - 1) * 2;
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
    return HU_OK;
#else
    if (!provider || !provider->vtable || !provider->vtable->chat_with_system)
        return HU_ERR_NOT_SUPPORTED;
    if (!problem || problem_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    /* 1. Generate thoughts via chat_with_system */
    char *gen_sys = hu_sprintf(alloc, TOT_GEN_SYS, num_branches);
    if (!gen_sys)
        return HU_ERR_OUT_OF_MEMORY;

    char *gen_out = NULL;
    size_t gen_out_len = 0;
    const char *gen_model = (model && model_len > 0) ? model : "gpt-4o-mini";
    size_t gen_model_len = (model && model_len > 0) ? model_len : 11;

    hu_error_t err = provider->vtable->chat_with_system(
        provider->ctx, alloc, gen_sys, strlen(gen_sys), problem, problem_len,
        gen_model, gen_model_len, 0.7, &gen_out, &gen_out_len);
    hu_str_free(alloc, gen_sys);

    if (err != HU_OK) {
        if (gen_out)
            alloc->free(alloc->ctx, gen_out, gen_out_len + 1);
        return err;
    }
    if (!gen_out || gen_out_len == 0) {
        if (gen_out)
            alloc->free(alloc->ctx, gen_out, gen_out_len + 1);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    /* 2. Parse thoughts from response */
    hu_tot_branch_t branches[HU_TOT_MAX_BRANCHES];
    memset(branches, 0, sizeof(branches));
    size_t branch_count =
        parse_thoughts(alloc, gen_out, gen_out_len, branches, HU_TOT_MAX_BRANCHES, 0);
    alloc->free(alloc->ctx, gen_out, gen_out_len + 1);

    if (branch_count == 0) {
        result->branches_explored = 0;
        result->branches_pruned = 0;
        return HU_OK;
    }

    const char *eval_sys = TOT_EVAL_SYS;
    size_t eval_sys_len = sizeof(TOT_EVAL_SYS) - 1;
    const char *eval_model = (model && model_len > 0) ? model : "gpt-4o-mini";
    size_t eval_model_len = (model && model_len > 0) ? model_len : 11;

    size_t pruned = 0;
    size_t total_explored = branch_count;
    int best_idx = -1;
    double best_score = -1.0;
    char *current_best = NULL;

    for (size_t i = 0; i < branch_count; i++) {
        size_t user_cap = branches[i].thought_len + 64;
        char *user_msg = (char *)alloc->alloc(alloc->ctx, user_cap);
        if (!user_msg) {
            for (size_t j = 0; j < branch_count; j++)
                if (branches[j].thought)
                    hu_str_free(alloc, branches[j].thought);
            return HU_ERR_OUT_OF_MEMORY;
        }
        int n = snprintf(user_msg, user_cap, "Rate this approach on a scale of 0.0 to 1.0: %s",
                         branches[i].thought);
        size_t user_len = (n > 0 && (size_t)n < user_cap) ? (size_t)n : strlen(user_msg);

        char *eval_out = NULL;
        size_t eval_out_len = 0;
        err = provider->vtable->chat_with_system(
            provider->ctx, alloc, eval_sys, eval_sys_len, user_msg, user_len,
            eval_model, eval_model_len, 0.0, &eval_out, &eval_out_len);
        alloc->free(alloc->ctx, user_msg, user_cap);

        if (err == HU_OK && eval_out && eval_out_len > 0) {
            double score = parse_score(eval_out, eval_out_len);
            if (score >= 0.0)
                branches[i].score = score;
        }
        if (eval_out)
            alloc->free(alloc->ctx, eval_out, eval_out_len + 1);

        if (branches[i].score < prune_threshold)
            pruned++;
        else if (branches[i].score > best_score) {
            best_score = branches[i].score;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0)
        current_best = hu_strdup(alloc, branches[best_idx].thought);

    int max_depth = config ? config->max_depth : 2;
    if (max_depth > 1 && current_best && total_explored < HU_TOT_MAX_TOTAL_NODES) {
        for (int depth = 1; depth < max_depth; depth++) {
            char *expand_sys = hu_sprintf(alloc, TOT_EXPAND_SYS, current_best, num_branches);
            if (!expand_sys)
                break;

            char *expand_out = NULL;
            size_t expand_out_len = 0;
            err = provider->vtable->chat_with_system(
                provider->ctx, alloc, expand_sys, strlen(expand_sys), problem, problem_len,
                gen_model, gen_model_len, 0.7, &expand_out, &expand_out_len);
            hu_str_free(alloc, expand_sys);

            if (err != HU_OK || !expand_out || expand_out_len == 0) {
                if (expand_out)
                    alloc->free(alloc->ctx, expand_out, expand_out_len + 1);
                break;
            }

            hu_tot_branch_t sub_branches[HU_TOT_MAX_BRANCHES];
            memset(sub_branches, 0, sizeof(sub_branches));
            size_t sub_count = parse_thoughts(alloc, expand_out, expand_out_len, sub_branches,
                                             HU_TOT_MAX_BRANCHES, depth);
            alloc->free(alloc->ctx, expand_out, expand_out_len + 1);

            if (sub_count == 0)
                break;

            for (size_t i = 0; i < sub_count; i++) {
                size_t user_cap = sub_branches[i].thought_len + 64;
                char *user_msg = (char *)alloc->alloc(alloc->ctx, user_cap);
                if (!user_msg) {
                    for (size_t j = 0; j < sub_count; j++)
                        if (sub_branches[j].thought)
                            hu_str_free(alloc, sub_branches[j].thought);
                    hu_str_free(alloc, current_best);
                    for (size_t j = 0; j < branch_count; j++)
                        if (branches[j].thought)
                            hu_str_free(alloc, branches[j].thought);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                int sn = snprintf(user_msg, user_cap,
                                 "Rate this approach on a scale of 0.0 to 1.0: %s",
                                 sub_branches[i].thought);
                size_t user_len = (sn > 0 && (size_t)sn < user_cap) ? (size_t)sn : strlen(user_msg);

                char *eval_out = NULL;
                size_t eval_out_len = 0;
                err = provider->vtable->chat_with_system(
                    provider->ctx, alloc, eval_sys, eval_sys_len, user_msg, user_len,
                    eval_model, eval_model_len, 0.0, &eval_out, &eval_out_len);
                alloc->free(alloc->ctx, user_msg, user_cap);

                if (err == HU_OK && eval_out && eval_out_len > 0) {
                    double score = parse_score(eval_out, eval_out_len);
                    if (score >= 0.0)
                        sub_branches[i].score = score;
                }
                if (eval_out)
                    alloc->free(alloc->ctx, eval_out, eval_out_len + 1);

                total_explored++;
                if (sub_branches[i].score < prune_threshold)
                    pruned++;
                else if (sub_branches[i].score > best_score) {
                    char *new_best = hu_strdup(alloc, sub_branches[i].thought);
                    if (new_best) {
                        hu_str_free(alloc, current_best);
                        current_best = new_best;
                        best_score = sub_branches[i].score;
                    }
                }
            }

            for (size_t j = 0; j < sub_count; j++)
                if (sub_branches[j].thought)
                    hu_str_free(alloc, sub_branches[j].thought);

            if (total_explored >= HU_TOT_MAX_TOTAL_NODES)
                break;
        }
    }

    result->branches_explored = total_explored;
    result->branches_pruned = pruned;

    if (current_best) {
        result->best_thought = current_best;
        result->best_thought_len = strlen(current_best);
        result->best_score = best_score;
    }

    for (size_t j = 0; j < branch_count; j++)
        if (branches[j].thought)
            hu_str_free(alloc, branches[j].thought);

    return HU_OK;
#endif
}
