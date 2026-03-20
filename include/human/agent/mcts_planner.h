#ifndef HU_AGENT_MCTS_PLANNER_H
#define HU_AGENT_MCTS_PLANNER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hu_mcts_config {
    int max_iterations;       /* default 100 */
    int max_depth;            /* default 5 */
    double exploration_c;     /* UCB1 constant, default 1.41 */
    int64_t max_time_ms;      /* time budget, default 5000 */
    int max_llm_calls;        /* LLM call budget, default 20 */
    hu_provider_t *provider;  /* optional: NULL = use heuristics */
    const char *model;
    size_t model_len;
} hu_mcts_config_t;

typedef struct hu_mcts_node {
    char action[512];
    size_t action_len;
    double total_value;
    int visit_count;
    int parent_idx;       /* index into nodes array, -1 for root */
    int children_start;   /* index of first child in nodes array */
    int children_count;
    int depth;
} hu_mcts_node_t;

typedef struct hu_mcts_result {
    char best_action[512];
    size_t best_action_len;
    double best_value; /* mean rollout value of best root child (first action) */
    int total_iterations;
    int total_nodes;
    int max_depth_reached;
    int llm_calls_used;
    /* Best path from root through highest-mean children (heap-owned; free via hu_mcts_result_free_path) */
    char **actions;
    size_t *action_lens;
    size_t action_count;
} hu_mcts_result_t;

hu_mcts_config_t hu_mcts_config_default(void);

void hu_mcts_result_free_path(hu_allocator_t *alloc, hu_mcts_result_t *result);

hu_error_t hu_mcts_plan(hu_allocator_t *alloc, const char *goal, size_t goal_len,
                       const char *context, size_t context_len,
                       const hu_mcts_config_t *config, hu_mcts_result_t *result);

#endif
