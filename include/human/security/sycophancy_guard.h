#ifndef HU_SECURITY_SYCOPHANCY_GUARD_H
#define HU_SECURITY_SYCOPHANCY_GUARD_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Anti-sycophancy guard — Silicon Mirror-inspired generation-time enforcement.
 *
 * Detects sycophantic patterns in generated responses and triggers rewrites.
 * Three factors from the Social Sycophancy Scale:
 * 1. Uncritical Agreement — blindly affirming user positions
 * 2. Obsequiousness — excessive deference and praise
 * 3. Excitement — inappropriate enthusiasm matching
 *
 * Pipeline: response → trait classification → risk scoring → rewrite if flagged.
 */

#define HU_SYCOPHANCY_PATTERN_MAX 32

typedef enum hu_sycophancy_factor {
    HU_SYCOPHANCY_UNCRITICAL_AGREEMENT = 0,
    HU_SYCOPHANCY_OBSEQUIOUSNESS,
    HU_SYCOPHANCY_EXCITEMENT,
    HU_SYCOPHANCY_FACTOR_COUNT,
} hu_sycophancy_factor_t;

typedef struct hu_sycophancy_result {
    float factor_scores[HU_SYCOPHANCY_FACTOR_COUNT]; /* 0.0–1.0 per factor */
    float total_risk;            /* weighted composite 0.0–1.0 */
    bool flagged;                /* true if total_risk >= threshold */
    size_t pattern_count;        /* number of sycophantic patterns detected */
} hu_sycophancy_result_t;

/*
 * Analyze a response for sycophantic patterns.
 * Considers both the response and the user message it responds to.
 * threshold: risk level above which flagged=true (default 0.5).
 */
hu_error_t hu_sycophancy_check(const char *response, size_t response_len,
                               const char *user_message, size_t user_len,
                               float threshold,
                               hu_sycophancy_result_t *result);

/*
 * Build a "necessary friction" rewrite directive for the model.
 * When a response is flagged, this generates instruction text that guides
 * the model to express genuine opinions rather than reflexive agreement.
 * Caller frees *out.
 */
hu_error_t hu_sycophancy_build_friction(hu_allocator_t *alloc,
                                        const hu_sycophancy_result_t *result,
                                        const char *user_message, size_t user_len,
                                        char **out, size_t *out_len);

#endif
