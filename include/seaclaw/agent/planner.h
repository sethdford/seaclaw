#ifndef SC_AGENT_PLANNER_H
#define SC_AGENT_PLANNER_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "seaclaw/provider.h"
#include <stdbool.h>
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Agent Planner — lightweight planning from structured JSON
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum sc_plan_step_status {
    SC_PLAN_STEP_PENDING,
    SC_PLAN_STEP_RUNNING,
    SC_PLAN_STEP_DONE,
    SC_PLAN_STEP_FAILED,
} sc_plan_step_status_t;

typedef struct sc_plan_step {
    char *tool_name;   /* owned */
    char *args_json;   /* owned; JSON object string */
    char *description; /* optional, owned */
    sc_plan_step_status_t status;
} sc_plan_step_t;

typedef struct sc_plan {
    sc_plan_step_t *steps; /* owned array */
    size_t steps_count;
    size_t steps_cap;
} sc_plan_t;

/* Create plan from structured JSON. Expected format:
 *   {"steps": [{"tool": "name", "args": {...}, "description": "..."}, ...]}
 * or {"steps": [{"name": "tool_name", "arguments": {...}}, ...]}
 * Caller must call sc_plan_free. */
sc_error_t sc_planner_create_plan(sc_allocator_t *alloc, const char *goal_json,
                                  size_t goal_json_len, sc_plan_t **out);

/* Generate a plan from a natural language goal by asking the LLM to decompose it.
 * Uses the provider to produce structured JSON, then parses it via sc_planner_create_plan.
 * tool_names: array of available tool names (used in the system prompt).
 * Caller must call sc_plan_free on the result. */
sc_error_t sc_planner_generate(sc_allocator_t *alloc, sc_provider_t *provider, const char *model,
                               size_t model_len, const char *goal, size_t goal_len,
                               const char *const *tool_names, size_t tool_count, sc_plan_t **out);

/* Generate a revised plan after a step failure. Takes the original goal,
 * what has succeeded so far, and what failed. Returns a new plan.
 * Under SC_IS_TEST, returns a minimal stub plan. */
sc_error_t sc_planner_replan(sc_allocator_t *alloc, sc_provider_t *provider, const char *model,
                             size_t model_len, const char *original_goal, size_t original_goal_len,
                             const char *progress_summary, size_t progress_summary_len,
                             const char *failure_detail, size_t failure_detail_len,
                             const char *const *tool_names, size_t tool_count, sc_plan_t **out);

/* Get next pending step, or NULL if none. Does not modify step status. */
sc_plan_step_t *sc_planner_next_step(const sc_plan_t *plan);

/* Mark step at index as done or failed. */
void sc_planner_mark_step(sc_plan_t *plan, size_t index, sc_plan_step_status_t status);

/* Check if all steps are done or failed (no pending/running). */
bool sc_planner_is_complete(const sc_plan_t *plan);

/* Free plan and all owned strings. */
void sc_plan_free(sc_allocator_t *alloc, sc_plan_t *plan);

#endif /* SC_AGENT_PLANNER_H */
