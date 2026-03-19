#ifndef HU_AGENT_PROCESS_REWARD_H
#define HU_AGENT_PROCESS_REWARD_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>

/* text aliases into the reasoning buffer passed to hu_prm_score_chain.
 * Caller must keep reasoning alive until hu_prm_result_free. */
typedef struct hu_prm_step {
    const char *text;
    size_t text_len;
    double score;
    bool is_correct;
} hu_prm_step_t;

typedef struct hu_prm_config {
    bool enabled;
    hu_provider_t *provider;
    const char *model;
    size_t model_len;
    double correctness_threshold;
} hu_prm_config_t;

typedef struct hu_prm_result {
    hu_prm_step_t *steps;
    size_t step_count;
    double aggregate_score;
    bool chain_valid;
} hu_prm_result_t;

hu_prm_config_t hu_prm_config_default(void);

hu_error_t hu_prm_score_chain(hu_allocator_t *alloc, const hu_prm_config_t *config,
                              const char *reasoning, size_t reasoning_len,
                              hu_prm_result_t *result);

hu_error_t hu_prm_score_step(hu_allocator_t *alloc, const hu_prm_config_t *config,
                             const char *step, size_t step_len,
                             const char *context, size_t context_len,
                             double *score);

void hu_prm_result_free(hu_allocator_t *alloc, hu_prm_result_t *result);

#endif /* HU_AGENT_PROCESS_REWARD_H */
