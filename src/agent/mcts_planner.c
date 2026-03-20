#include "human/agent/mcts_planner.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HU_MCTS_MAX_NODES 256

hu_mcts_config_t hu_mcts_config_default(void) {
    hu_mcts_config_t c;
    memset(&c, 0, sizeof(c));
    c.max_iterations = 100;
    c.max_depth = 5;
    c.exploration_c = 1.41;
    c.max_time_ms = 5000;
    c.max_llm_calls = 20;
    return c;
}

#ifdef HU_IS_TEST
/* Mock expansion: generate 2-3 actions per node. */
static int mock_expand(hu_mcts_node_t *nodes, int *node_count, int parent_idx,
                      int depth, int child_offset) {
    const char *actions[] = {"Plan step A", "Plan step B", "Plan step C"};
    int num_actions = 2 + (depth % 2);
    if (num_actions > 3)
        num_actions = 3;
    for (int i = 0; i < num_actions && *node_count < HU_MCTS_MAX_NODES; i++) {
        int idx = (*node_count)++;
        hu_mcts_node_t *node = &nodes[idx];
        memset(node, 0, sizeof(*node));
        size_t len = strlen(actions[i]);
        if (len >= sizeof(node->action))
            len = sizeof(node->action) - 1;
        memcpy(node->action, actions[i], len);
        node->action_len = len;
        node->action[len] = '\0';
        node->parent_idx = parent_idx;
        node->depth = depth;
        node->children_start = -1;
        node->children_count = 0;
    }
    if (parent_idx >= 0) {
        nodes[parent_idx].children_start = child_offset;
        nodes[parent_idx].children_count = num_actions;
    }
    return num_actions;
}

/* Mock simulation: value = 0.8 - 0.1 * depth (deeper = slightly worse). */
static double mock_simulate(int depth) {
    double v = 0.8 - 0.1 * (double)depth;
    return v < 0.0 ? 0.0 : v;
}
#endif

void hu_mcts_result_free_path(hu_allocator_t *alloc, hu_mcts_result_t *result) {
    if (!alloc || !result)
        return;
    if (result->actions && result->action_count > 0) {
        for (size_t i = 0; i < result->action_count; i++) {
            if (result->actions[i])
                alloc->free(alloc->ctx, result->actions[i], result->action_lens[i] + 1);
        }
        alloc->free(alloc->ctx, result->actions, result->action_count * sizeof(char *));
    }
    if (result->action_lens)
        alloc->free(alloc->ctx, result->action_lens, result->action_count * sizeof(size_t));
    result->actions = NULL;
    result->action_lens = NULL;
    result->action_count = 0;
}

/* Walk from root's best first child, always choosing the visited child with highest mean value. */
static hu_error_t mcts_build_best_path(hu_allocator_t *alloc, hu_mcts_node_t *nodes, int node_count,
                                       int root_best_child, hu_mcts_result_t *result) {
    if (!alloc || !nodes || !result || root_best_child < 0)
        return HU_OK;

    int path_nodes[HU_MCTS_MAX_NODES];
    size_t path_len = 0;
    int cur = root_best_child;

    while (cur >= 0 && path_len < HU_MCTS_MAX_NODES) {
        if (nodes[cur].action_len > 0)
            path_nodes[path_len++] = cur;

        hu_mcts_node_t *n = &nodes[cur];
        if (n->children_count <= 0)
            break;

        int best_next = -1;
        double best_mean = -1.0;
        for (int i = 0; i < n->children_count; i++) {
            int cidx = n->children_start + i;
            if (cidx < 0 || cidx >= node_count)
                break;
            hu_mcts_node_t *c = &nodes[cidx];
            if (c->visit_count == 0)
                continue;
            double mean = c->total_value / (double)c->visit_count;
            if (best_next < 0 || mean > best_mean) {
                best_next = cidx;
                best_mean = mean;
            }
        }
        if (best_next < 0)
            break;
        cur = best_next;
    }

    if (path_len == 0)
        return HU_OK;

    char **acts = (char **)alloc->alloc(alloc->ctx, path_len * sizeof(char *));
    size_t *lens = (size_t *)alloc->alloc(alloc->ctx, path_len * sizeof(size_t));
    if (!acts || !lens) {
        if (acts)
            alloc->free(alloc->ctx, acts, path_len * sizeof(char *));
        if (lens)
            alloc->free(alloc->ctx, lens, path_len * sizeof(size_t));
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < path_len; i++) {
        hu_mcts_node_t *nd = &nodes[path_nodes[i]];
        size_t L = nd->action_len;
        char *copy = (char *)alloc->alloc(alloc->ctx, L + 1);
        if (!copy) {
            for (size_t j = 0; j < i; j++)
                alloc->free(alloc->ctx, acts[j], lens[j] + 1);
            alloc->free(alloc->ctx, acts, path_len * sizeof(char *));
            alloc->free(alloc->ctx, lens, path_len * sizeof(size_t));
            return HU_ERR_OUT_OF_MEMORY;
        }
        memcpy(copy, nd->action, L);
        copy[L] = '\0';
        acts[i] = copy;
        lens[i] = L;
    }

    result->actions = acts;
    result->action_lens = lens;
    result->action_count = path_len;
    return HU_OK;
}

/* UCB1 score: value/visits + C * sqrt(ln(parent_visits)/visits).
 * For unvisited (visit_count==0) return a large value to prefer exploration. */
static double ucb1_score(double total_value, int visit_count, int parent_visits,
                         double exploration_c) {
    if (visit_count == 0)
        return 1e9;
    double mean = total_value / (double)visit_count;
    double expl = exploration_c * sqrt(log((double)(parent_visits + 1)) / (double)visit_count);
    return mean + expl;
}

#ifndef HU_IS_TEST
/* Production expansion: use LLM when provider available and budget allows;
 * otherwise heuristic templates. */
static int prod_expand(hu_mcts_node_t *nodes, int *node_count, int parent_idx,
                       int depth, int child_offset, const char *goal, size_t goal_len,
                       hu_allocator_t *alloc, hu_provider_t *provider,
                       const char *model, size_t model_len,
                       int *llm_calls, int max_llm_calls) {
    if (provider && provider->vtable && provider->vtable->chat_with_system &&
        llm_calls && *llm_calls < max_llm_calls) {
        char system[256];
        int sys_len = snprintf(system, sizeof(system),
                               "You are a planning agent. Given a goal, propose 2-3 concrete next "
                               "actions. Output one action per line, nothing else.");
        if (sys_len < 0 || sys_len >= (int)sizeof(system))
            sys_len = (int)sizeof(system) - 1;

        size_t goal_trunc = goal_len > 200 ? 200 : goal_len;
        char prompt[512];
        int pn = snprintf(prompt, sizeof(prompt),
                          "Goal: %.*s\nCurrent depth: %d\nPropose actions:",
                          (int)goal_trunc, goal && goal_len > 0 ? goal : "", depth);
        if (pn < 0 || pn >= (int)sizeof(prompt))
            pn = (int)sizeof(prompt) - 1;

        char *response = NULL;
        size_t resp_len = 0;
        hu_error_t err = provider->vtable->chat_with_system(
            provider->ctx, alloc, system, (size_t)sys_len,
            prompt, (size_t)pn,
            model && model_len > 0 ? model : "gpt-4o-mini",
            model && model_len > 0 ? model_len : 11,
            0.2, &response, &resp_len);

        if (err == HU_OK && response && resp_len > 0) {
            (*llm_calls)++;
            int num_actions = 0;
            const char *p = response;
            const char *end = response + resp_len;
            while (p < end && num_actions < 3 && *node_count < HU_MCTS_MAX_NODES) {
                const char *line_start = p;
                while (p < end && *p != '\n' && *p != '\r')
                    p++;
                size_t line_len = (size_t)(p - line_start);
                while (p < end && (*p == '\n' || *p == '\r'))
                    p++;
                if (line_len > 0) {
                    while (line_len > 0 && (line_start[line_len - 1] == ' ' ||
                                            line_start[line_len - 1] == '\t'))
                        line_len--;
                    if (line_len > 0) {
                        int idx = (*node_count)++;
                        hu_mcts_node_t *node = &nodes[idx];
                        memset(node, 0, sizeof(*node));
                        size_t copy_len = line_len < sizeof(node->action) - 1
                                              ? line_len
                                              : sizeof(node->action) - 1;
                        memcpy(node->action, line_start, copy_len);
                        node->action[copy_len] = '\0';
                        node->action_len = copy_len;
                        node->parent_idx = parent_idx;
                        node->depth = depth;
                        node->children_start = -1;
                        node->children_count = 0;
                        num_actions++;
                    }
                }
            }
            alloc->free(alloc->ctx, response, resp_len + 1);
            if (num_actions > 0) {
                if (parent_idx >= 0) {
                    nodes[parent_idx].children_start = child_offset;
                    nodes[parent_idx].children_count = num_actions;
                }
                return num_actions;
            }
        }
        if (response)
            alloc->free(alloc->ctx, response, resp_len + 1);
    }

    /* Fallback: heuristic templates */
    char buf[512];
    size_t goal_copy_len = goal_len;
    if (goal_copy_len >= 64)
        goal_copy_len = 63;
    char goal_copy[64];
    if (goal && goal_len > 0) {
        memcpy(goal_copy, goal, goal_copy_len);
        goal_copy[goal_copy_len] = '\0';
    } else {
        goal_copy[0] = '\0';
    }
    const char *templates[] = {
        "Research and analyze %s",
        "Plan approach for %s",
        "Execute and verify %s",
    };
    int n = 3;
    if (depth >= 4)
        n = 2;
    for (int i = 0; i < n && *node_count < HU_MCTS_MAX_NODES; i++) {
        int len = (int)snprintf(buf, sizeof(buf), templates[i], goal_copy[0] ? goal_copy : "goal");
        if (len < 0 || (size_t)len >= sizeof(buf))
            len = (int)sizeof(buf) - 1;
        int idx = (*node_count)++;
        hu_mcts_node_t *node = &nodes[idx];
        memset(node, 0, sizeof(*node));
        size_t copy_len = (size_t)len;
        if (copy_len >= sizeof(node->action))
            copy_len = sizeof(node->action) - 1;
        memcpy(node->action, buf, copy_len);
        node->action[copy_len] = '\0';
        node->action_len = copy_len;
        node->parent_idx = parent_idx;
        node->depth = depth;
        node->children_start = -1;
        node->children_count = 0;
    }
    if (parent_idx >= 0) {
        nodes[parent_idx].children_start = child_offset;
        nodes[parent_idx].children_count = n;
    }
    return n;
}

/* Production simulation: use LLM for evaluation when provider available and
 * budget allows; otherwise heuristic (depth penalty + goal-word match). */
static double prod_simulate(int depth, const char *action, size_t action_len,
                            const char *goal, size_t goal_len,
                            hu_allocator_t *alloc, hu_provider_t *provider,
                            const char *model, size_t model_len,
                            int *llm_calls, int max_llm_calls) {
    if (provider && provider->vtable && provider->vtable->chat_with_system &&
        llm_calls && *llm_calls < max_llm_calls) {
        char system[256];
        int sys_len = snprintf(system, sizeof(system),
                               "You are a planning evaluator. Rate how good an action is for "
                               "achieving a goal. Reply with a single number 0.0 to 1.0 only.");
        if (sys_len < 0 || sys_len >= (int)sizeof(system))
            sys_len = (int)sizeof(system) - 1;

        size_t goal_trunc = goal_len > 100 ? 100 : goal_len;
        size_t action_trunc = action_len > 200 ? 200 : action_len;
        char prompt[512];
        int pn = snprintf(prompt, sizeof(prompt),
                          "Goal: %.*s\nAction (depth %d): %.*s\nScore (0.0-1.0):",
                          (int)goal_trunc, goal && goal_len > 0 ? goal : "", depth,
                          (int)action_trunc, action && action_len > 0 ? action : "");
        if (pn < 0 || pn >= (int)sizeof(prompt))
            pn = (int)sizeof(prompt) - 1;

        char *response = NULL;
        size_t resp_len = 0;
        hu_error_t err = provider->vtable->chat_with_system(
            provider->ctx, alloc, system, (size_t)sys_len,
            prompt, (size_t)pn,
            model && model_len > 0 ? model : "gpt-4o-mini",
            model && model_len > 0 ? model_len : 11,
            0.0, &response, &resp_len);

        if (err == HU_OK && response && resp_len > 0) {
            (*llm_calls)++;
            double v = strtod(response, NULL);
            alloc->free(alloc->ctx, response, resp_len + 1);
            if (v >= 0.0 && v <= 1.0)
                return v;
        }
        if (response)
            alloc->free(alloc->ctx, response, resp_len + 1);
    }

    /* Fallback: heuristic */
    double v = 0.8 - 0.05 * (double)depth;
    if (v < 0.0)
        v = 0.0;
    if (goal && goal_len > 0 && action && action_len > 0) {
        for (size_t i = 0; i + goal_len <= action_len; i++) {
            if (memcmp(action + i, goal, goal_len) == 0) {
                v += 0.1;
                break;
            }
        }
    }
    if (v > 1.0)
        v = 1.0;
    return v;
}
#endif /* !HU_IS_TEST */

hu_error_t hu_mcts_plan(hu_allocator_t *alloc, const char *goal, size_t goal_len,
                       const char *context, size_t context_len,
                       const hu_mcts_config_t *config, hu_mcts_result_t *result) {
    (void)alloc;
    (void)goal;
    (void)goal_len;
    (void)context;
    (void)context_len;

    if (!alloc || !goal || !result)
        return HU_ERR_INVALID_ARGUMENT;

    const hu_mcts_config_t *cfg = config ? config : &(hu_mcts_config_t){0};
    hu_mcts_config_t def = hu_mcts_config_default();
    int max_iter = cfg->max_iterations > 0 ? cfg->max_iterations : def.max_iterations;
    int max_depth = cfg->max_depth > 0 ? cfg->max_depth : def.max_depth;
    double exploration_c = cfg->exploration_c > 0.0 ? cfg->exploration_c : def.exploration_c;
    int max_llm_calls = cfg->max_llm_calls > 0 ? cfg->max_llm_calls : def.max_llm_calls;
    hu_provider_t *provider = cfg->provider;
    const char *model = cfg->model;
    size_t model_len = cfg->model_len;

    memset(result, 0, sizeof(*result));

#ifdef HU_IS_TEST
    hu_mcts_node_t nodes[HU_MCTS_MAX_NODES];
    memset(nodes, 0, sizeof(nodes));
    int node_count = 0;

    /* Root node */
    nodes[0].action[0] = '\0';
    nodes[0].action_len = 0;
    nodes[0].parent_idx = -1;
    nodes[0].depth = 0;
    nodes[0].children_start = -1;
    nodes[0].children_count = 0;
    node_count = 1;

    int llm_calls = 0;
    int max_depth_reached = 0;

    for (int iter = 0; iter < max_iter; iter++) {
        /* Selection: traverse from root using UCB1 until we hit a leaf or max depth */
        int node_idx = 0;
        while (node_idx >= 0 && nodes[node_idx].depth < max_depth) {
            hu_mcts_node_t *n = &nodes[node_idx];
            if (n->children_count == 0) {
                /* Expansion: generate children */
                int child_start = node_count;
                int added = mock_expand(nodes, &node_count, node_idx, n->depth + 1, child_start);
                (void)added;
                llm_calls++;
                if (node_count >= HU_MCTS_MAX_NODES)
                    break;
                /* After expansion, this node has children; pick first unvisited */
                int best_child = -1;
                double best_score = -1.0;
                for (int i = 0; i < n->children_count; i++) {
                    int cidx = n->children_start + i;
                    if (cidx >= node_count)
                        break;
                    double s = ucb1_score(nodes[cidx].total_value, nodes[cidx].visit_count,
                                         n->visit_count, exploration_c);
                    if (best_child < 0 || s > best_score) {
                        best_child = cidx;
                        best_score = s;
                    }
                }
                if (best_child >= 0)
                    node_idx = best_child;
                else
                    break;
            } else {
                /* Select child with highest UCB1 */
                int best_child = -1;
                double best_score = -1.0;
                for (int i = 0; i < n->children_count; i++) {
                    int cidx = n->children_start + i;
                    if (cidx >= node_count)
                        break;
                    double s = ucb1_score(nodes[cidx].total_value, nodes[cidx].visit_count,
                                         n->visit_count, exploration_c);
                    if (best_child < 0 || s > best_score) {
                        best_child = cidx;
                        best_score = s;
                    }
                }
                if (best_child >= 0)
                    node_idx = best_child;
                else
                    break;
            }
        }

        if (node_idx < 0)
            break;

        /* Simulation: get value for this leaf */
        int sim_depth = nodes[node_idx].depth;
        if (sim_depth > max_depth_reached)
            max_depth_reached = sim_depth;
        double value = mock_simulate(sim_depth);

        /* Backpropagation: update value and visit_count up to root */
        int idx = node_idx;
        while (idx >= 0) {
            nodes[idx].total_value += value;
            nodes[idx].visit_count++;
            idx = nodes[idx].parent_idx;
        }
    }

    /* Choose best action: root's child with highest mean value (or most visits) */
    int best_child = -1;
    double best_mean = -1.0;
    hu_mcts_node_t *root = &nodes[0];
    for (int i = 0; i < root->children_count; i++) {
        int cidx = root->children_start + i;
        if (cidx >= node_count)
            break;
        hu_mcts_node_t *c = &nodes[cidx];
        if (c->visit_count == 0)
            continue;
        double mean = c->total_value / (double)c->visit_count;
        if (best_child < 0 || mean > best_mean) {
            best_child = cidx;
            best_mean = mean;
        }
    }

    if (best_child >= 0) {
        hu_mcts_node_t *best = &nodes[best_child];
        size_t len = best->action_len;
        if (len >= sizeof(result->best_action))
            len = sizeof(result->best_action) - 1;
        memcpy(result->best_action, best->action, len);
        result->best_action[len] = '\0';
        result->best_action_len = len;
        result->best_value = best_mean;
        if (mcts_build_best_path(alloc, nodes, node_count, best_child, result) != HU_OK)
            hu_mcts_result_free_path(alloc, result);
    } else if (root->visit_count > 0) {
        result->best_action[0] = '\0';
        result->best_action_len = 0;
        result->best_value = root->total_value / (double)root->visit_count;
    }

    result->total_iterations = max_iter;
    result->total_nodes = node_count;
    result->max_depth_reached = max_depth_reached;
    result->llm_calls_used = llm_calls;

    return HU_OK;
#else
    /* Production: heuristic expansion and simulation (no LLM). */
    hu_mcts_node_t nodes[HU_MCTS_MAX_NODES];
    memset(nodes, 0, sizeof(nodes));
    int node_count = 0;

    nodes[0].action[0] = '\0';
    nodes[0].action_len = 0;
    nodes[0].parent_idx = -1;
    nodes[0].depth = 0;
    nodes[0].children_start = -1;
    nodes[0].children_count = 0;
    node_count = 1;

    int llm_calls = 0;
    int max_depth_reached = 0;

    for (int iter = 0; iter < max_iter; iter++) {
        int node_idx = 0;
        while (node_idx >= 0 && nodes[node_idx].depth < max_depth) {
            hu_mcts_node_t *n = &nodes[node_idx];
            if (n->children_count == 0) {
                int child_start = node_count;
                prod_expand(nodes, &node_count, node_idx, n->depth + 1, child_start,
                           goal, goal_len, alloc, provider, model, model_len,
                           &llm_calls, max_llm_calls);
                if (node_count >= HU_MCTS_MAX_NODES)
                    break;
                int best_child = -1;
                double best_score = -1.0;
                for (int i = 0; i < n->children_count; i++) {
                    int cidx = n->children_start + i;
                    if (cidx >= node_count)
                        break;
                    double s = ucb1_score(nodes[cidx].total_value, nodes[cidx].visit_count,
                                         n->visit_count, exploration_c);
                    if (best_child < 0 || s > best_score) {
                        best_child = cidx;
                        best_score = s;
                    }
                }
                if (best_child >= 0)
                    node_idx = best_child;
                else
                    break;
            } else {
                int best_child = -1;
                double best_score = -1.0;
                for (int i = 0; i < n->children_count; i++) {
                    int cidx = n->children_start + i;
                    if (cidx >= node_count)
                        break;
                    double s = ucb1_score(nodes[cidx].total_value, nodes[cidx].visit_count,
                                         n->visit_count, exploration_c);
                    if (best_child < 0 || s > best_score) {
                        best_child = cidx;
                        best_score = s;
                    }
                }
                if (best_child >= 0)
                    node_idx = best_child;
                else
                    break;
            }
        }

        if (node_idx < 0)
            break;

        int sim_depth = nodes[node_idx].depth;
        if (sim_depth > max_depth_reached)
            max_depth_reached = sim_depth;
        double value = prod_simulate(sim_depth, nodes[node_idx].action, nodes[node_idx].action_len,
                                     goal, goal_len, alloc, provider, model, model_len,
                                     &llm_calls, max_llm_calls);

        int idx = node_idx;
        while (idx >= 0) {
            nodes[idx].total_value += value;
            nodes[idx].visit_count++;
            idx = nodes[idx].parent_idx;
        }
    }

    int best_child = -1;
    double best_mean = -1.0;
    hu_mcts_node_t *root = &nodes[0];
    for (int i = 0; i < root->children_count; i++) {
        int cidx = root->children_start + i;
        if (cidx >= node_count)
            break;
        hu_mcts_node_t *c = &nodes[cidx];
        if (c->visit_count == 0)
            continue;
        double mean = c->total_value / (double)c->visit_count;
        if (best_child < 0 || mean > best_mean) {
            best_child = cidx;
            best_mean = mean;
        }
    }

    if (best_child >= 0) {
        hu_mcts_node_t *best = &nodes[best_child];
        size_t len = best->action_len;
        if (len >= sizeof(result->best_action))
            len = sizeof(result->best_action) - 1;
        memcpy(result->best_action, best->action, len);
        result->best_action[len] = '\0';
        result->best_action_len = len;
        result->best_value = best_mean;
        if (mcts_build_best_path(alloc, nodes, node_count, best_child, result) != HU_OK)
            hu_mcts_result_free_path(alloc, result);
    } else if (root->visit_count > 0) {
        result->best_action[0] = '\0';
        result->best_action_len = 0;
        result->best_value = root->total_value / (double)root->visit_count;
    }

    result->total_iterations = max_iter;
    result->total_nodes = node_count;
    result->max_depth_reached = max_depth_reached;
    result->llm_calls_used = llm_calls;

    return HU_OK;
#endif
}
