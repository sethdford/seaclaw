#ifndef HU_SECURITY_HISTORY_SCORER_H
#define HU_SECURITY_HISTORY_SCORER_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Heuristic multi-step tool-call pattern detector.
 * Flags sequences where risk levels increase monotonically, suggesting
 * privilege escalation probing. Uses simple risk-level comparison and
 * data-flow keyword matching, not true attack-graph reasoning.
 * Effective as a lightweight anomaly signal; should be combined with
 * policy enforcement for defense-in-depth.
 * Inspired by: arXiv:2601.10156 (ToolSafe) */

typedef struct hu_tool_history_entry {
    const char *tool_name;
    size_t name_len;
    bool succeeded;
    uint32_t risk_level;
} hu_tool_history_entry_t;

typedef struct hu_history_score_result {
    double escalation_score; /* [0.0, 1.0] — likelihood of escalation attack */
    bool is_suspicious;
    char pattern[128];
    size_t pattern_len;
} hu_history_score_result_t;

/* Analyze tool call history for multi-step attack patterns */
hu_error_t hu_history_scorer_evaluate(const hu_tool_history_entry_t *history, size_t count,
                                      const char *proposed_tool, size_t tool_len,
                                      uint32_t proposed_risk, hu_history_score_result_t *out);

#endif
