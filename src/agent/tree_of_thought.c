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
    "reasoning approaches. Each approach must be on its own line, prefixed with "
    "\"THOUGHT N:\" where N is 1, 2, 3, etc. Be concise. No other text.";

static TOT_UNUSED const char TOT_EVAL_SYS[] =
    "Rate how promising this reasoning path is for solving the problem, from 0.0 to 1.0. "
    "Reply with ONLY a number (e.g. 0.85). No explanation.";

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

/* Parse "THOUGHT N: content" lines from response. Returns count of branches parsed. */
static TOT_UNUSED size_t parse_thoughts(hu_allocator_t *alloc, const char *resp, size_t resp_len,
                             hu_tot_branch_t *branches, size_t max_branches) {
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
                branches[count].depth = 0;
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
    /* Deterministic stub: no LLM calls, return mock branches with predetermined scores */
    (void)provider;
    (void)model;
    (void)model_len;
    (void)problem;
    (void)problem_len;
    (void)config;

    static const char *mock_thoughts[] = {"Break into subproblems", "Use analogy", "Work backwards"};
    static const double mock_scores[] = {0.9, 0.6, 0.25};
    size_t n = (size_t)num_branches;
    if (n > 3)
        n = 3;

    size_t pruned = 0;
    int best_idx = -1;
    double best_score = -1.0;
    for (size_t i = 0; i < n; i++) {
        if (mock_scores[i] < prune_threshold)
            pruned++;
        else if (mock_scores[i] > best_score) {
            best_score = mock_scores[i];
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0) {
        result->best_thought = hu_strdup(alloc, mock_thoughts[best_idx]);
        if (result->best_thought) {
            result->best_thought_len = strlen(result->best_thought);
            result->best_score = mock_scores[best_idx];
        }
    }
    result->branches_explored = n;
    result->branches_pruned = pruned;
    return HU_OK;
#else
    if (!provider || !provider->vtable || !provider->vtable->chat)
        return HU_ERR_NOT_SUPPORTED;
    if (!problem || problem_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    /* 1. Build generate-thoughts prompt */
    char *gen_sys = hu_sprintf(alloc, TOT_GEN_SYS, num_branches);
    if (!gen_sys)
        return HU_ERR_OUT_OF_MEMORY;

    hu_chat_message_t gen_msgs[2];
    memset(gen_msgs, 0, sizeof(gen_msgs));
    gen_msgs[0].role = HU_ROLE_SYSTEM;
    gen_msgs[0].content = gen_sys;
    gen_msgs[0].content_len = strlen(gen_sys);
    gen_msgs[1].role = HU_ROLE_USER;
    gen_msgs[1].content = problem;
    gen_msgs[1].content_len = problem_len;

    hu_chat_request_t gen_req;
    memset(&gen_req, 0, sizeof(gen_req));
    gen_req.messages = gen_msgs;
    gen_req.messages_count = 2;
    gen_req.model = model;
    gen_req.model_len = model_len;
    gen_req.temperature = 0.7;

    hu_chat_response_t gen_resp;
    memset(&gen_resp, 0, sizeof(gen_resp));
    hu_error_t err =
        provider->vtable->chat(provider->ctx, alloc, &gen_req, model, model_len, 0.7, &gen_resp);
    hu_str_free(alloc, gen_sys);

    if (err != HU_OK) {
        hu_chat_response_free(alloc, &gen_resp);
        return err;
    }
    if (!gen_resp.content || gen_resp.content_len == 0) {
        hu_chat_response_free(alloc, &gen_resp);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    /* 2. Parse thoughts from response */
    hu_tot_branch_t branches[HU_TOT_MAX_BRANCHES];
    memset(branches, 0, sizeof(branches));
    size_t branch_count =
        parse_thoughts(alloc, gen_resp.content, gen_resp.content_len, branches, HU_TOT_MAX_BRANCHES);
    hu_chat_response_free(alloc, &gen_resp);

    if (branch_count == 0) {
        result->branches_explored = 0;
        result->branches_pruned = 0;
        return HU_OK;
    }

    /* 3. Evaluate each thought */
    const char *eval_sys = TOT_EVAL_SYS;
    size_t eval_sys_len = sizeof(TOT_EVAL_SYS) - 1;

    size_t pruned = 0;
    int best_idx = -1;
    double best_score = -1.0;

    for (size_t i = 0; i < branch_count; i++) {
        /* Build eval user message: problem + thought */
        size_t user_cap = problem_len + branches[i].thought_len + 64;
        char *user_msg = (char *)alloc->alloc(alloc->ctx, user_cap);
        if (!user_msg) {
            for (size_t j = 0; j < branch_count; j++)
                if (branches[j].thought)
                    hu_str_free(alloc, branches[j].thought);
            return HU_ERR_OUT_OF_MEMORY;
        }
        int n = snprintf(user_msg, user_cap, "Problem: %.*s\n\nThought to evaluate: %s",
                         (int)problem_len, problem, branches[i].thought);
        size_t user_len = (n > 0 && (size_t)n < user_cap) ? (size_t)n : strlen(user_msg);

        hu_chat_message_t eval_msgs[2];
        memset(eval_msgs, 0, sizeof(eval_msgs));
        eval_msgs[0].role = HU_ROLE_SYSTEM;
        eval_msgs[0].content = eval_sys;
        eval_msgs[0].content_len = eval_sys_len;
        eval_msgs[1].role = HU_ROLE_USER;
        eval_msgs[1].content = user_msg;
        eval_msgs[1].content_len = user_len;

        hu_chat_request_t eval_req;
        memset(&eval_req, 0, sizeof(eval_req));
        eval_req.messages = eval_msgs;
        eval_req.messages_count = 2;
        eval_req.model = model;
        eval_req.model_len = model_len;
        eval_req.temperature = 0.0;

        hu_chat_response_t eval_resp;
        memset(&eval_resp, 0, sizeof(eval_resp));
        err = provider->vtable->chat(provider->ctx, alloc, &eval_req, model, model_len, 0.0,
                                     &eval_resp);
        alloc->free(alloc->ctx, user_msg, user_cap);

        if (err == HU_OK && eval_resp.content && eval_resp.content_len > 0) {
            double score = parse_score(eval_resp.content, eval_resp.content_len);
            if (score >= 0.0)
                branches[i].score = score;
        }
        hu_chat_response_free(alloc, &eval_resp);

        if (branches[i].score < prune_threshold)
            pruned++;
        else if (branches[i].score > best_score) {
            best_score = branches[i].score;
            best_idx = (int)i;
        }
    }

    result->branches_explored = branch_count;
    result->branches_pruned = pruned;

    if (best_idx >= 0) {
        result->best_thought = hu_strdup(alloc, branches[best_idx].thought);
        if (result->best_thought) {
            result->best_thought_len = strlen(result->best_thought);
            result->best_score = branches[best_idx].score;
        }
    }

    for (size_t j = 0; j < branch_count; j++)
        if (branches[j].thought)
            hu_str_free(alloc, branches[j].thought);

    return HU_OK;
#endif
}
