#ifndef HU_AGENT_TREE_OF_THOUGHT_H
#define HU_AGENT_TREE_OF_THOUGHT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Tree-of-Thought — explores multiple reasoning branches for complex problems.
 * Generates N candidate thought paths, evaluates each, and selects the best.
 * Based on Yao et al. (2023) "Tree of Thoughts" framework.
 */

#define HU_TOT_MAX_BRANCHES 5
#define HU_TOT_MAX_DEPTH 3

typedef struct hu_tot_branch {
    char *thought;
    size_t thought_len;
    double score; /* 0.0-1.0 evaluated quality */
    int depth;
} hu_tot_branch_t;

typedef struct hu_tot_result {
    char *best_thought;
    size_t best_thought_len;
    double best_score;
    size_t branches_explored;
    size_t branches_pruned;
    int max_depth_reached;
    int llm_calls_made;
} hu_tot_result_t;

#define HU_TOT_STRATEGY_BEAM_SEARCH 0
#define HU_TOT_STRATEGY_BEST_FIRST  1
#define HU_TOT_STRATEGY_DFS         2

typedef struct hu_tot_config {
    int num_branches;       /* candidates per level (default 3) */
    int max_depth;          /* reasoning depth (default 2) */
    double prune_threshold; /* discard branches below this score (default 0.3) */
    bool enabled;
    int beam_width;         /* top-K to keep at each level (default 3) */
    int max_total_nodes;    /* absolute node budget (default 50) */
    int strategy;           /* 0=beam_search, 1=best_first, 2=dfs */
} hu_tot_config_t;

/* Generate and evaluate multiple reasoning branches for a problem.
 * Uses the provider to generate thoughts and self-evaluate.
 * Caller must free result with hu_tot_result_free. */
hu_error_t hu_tot_explore(hu_allocator_t *alloc, hu_provider_t *provider,
                          const char *model, size_t model_len,
                          const char *problem, size_t problem_len,
                          const hu_tot_config_t *config,
                          hu_tot_result_t *result);

void hu_tot_result_free(hu_allocator_t *alloc, hu_tot_result_t *result);

hu_tot_config_t hu_tot_config_default(void);

#endif
