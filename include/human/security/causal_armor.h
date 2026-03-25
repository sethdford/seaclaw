#ifndef HU_SECURITY_CAUSAL_ARMOR_H
#define HU_SECURITY_CAUSAL_ARMOR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* CausalArmor: leave-one-out causal attribution for indirect prompt injection defense.
 * At privileged decision points, computes attribution margins to detect when untrusted
 * content (tool outputs, retrieved docs) has disproportionate influence over tool calls.
 * Reference: arXiv:2602.07918 (CausalArmor) */

typedef struct hu_causal_segment {
    const char *content;
    size_t content_len;
    bool is_trusted; /* true = user message, false = tool output/retrieved doc */
} hu_causal_segment_t;

typedef struct hu_causal_attribution {
    size_t segment_idx;
    double influence_score; /* [0.0, 1.0] — how much removing this segment changes the decision */
    bool is_dominant;       /* true if this segment alone drives the decision */
} hu_causal_attribution_t;

typedef struct hu_causal_armor_config {
    double dominance_threshold; /* attribution margin above which segment is "dominant" (default:
                                   0.6) */
    double user_intent_floor; /* minimum user-intent attribution for a safe action (default: 0.3) */
    size_t max_segments;      /* max segments to analyze (default: 8) */
} hu_causal_armor_config_t;

typedef struct hu_causal_armor_result {
    bool is_safe;
    double user_intent_attribution;   /* [0.0, 1.0] */
    double max_untrusted_attribution; /* [0.0, 1.0] */
    size_t dominant_segment_idx;      /* index of most influential untrusted segment, or SIZE_MAX */
    char reason[256];
    size_t reason_len;
} hu_causal_armor_result_t;

void hu_causal_armor_config_default(hu_causal_armor_config_t *cfg);

/* Compute causal attributions for a proposed tool call.
 * segments: the context segments (user messages + tool outputs).
 * proposed_tool: the tool the LLM wants to call.
 * This is a heuristic approximation — full leave-one-out requires model re-inference. */
hu_error_t hu_causal_armor_evaluate(const hu_causal_armor_config_t *cfg,
                                    const hu_causal_segment_t *segments, size_t segment_count,
                                    const char *proposed_tool, size_t tool_len,
                                    const char *proposed_args, size_t args_len,
                                    hu_causal_armor_result_t *out);

/* Check if a tool call mentions entities/actions only present in untrusted segments */
hu_error_t hu_causal_armor_check_grounding(const hu_causal_segment_t *segments,
                                           size_t segment_count, const char *tool_name,
                                           size_t name_len, const char *args, size_t args_len,
                                           double *grounding_score);

#endif
