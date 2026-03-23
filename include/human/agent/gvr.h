#ifndef HU_AGENT_GVR_H
#define HU_AGENT_GVR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_gvr_verdict {
    HU_GVR_PASS = 0,
    HU_GVR_FAIL,
} hu_gvr_verdict_t;

typedef struct hu_gvr_config {
    bool enabled;
    uint32_t max_revisions;          /* 0 = default (2) */
    const char *verifier_model;      /* NULL = use same model as generator */
    size_t verifier_model_len;
} hu_gvr_config_t;

#define HU_GVR_CONFIG_DEFAULT \
    { .enabled = false, .max_revisions = 2, .verifier_model = NULL, .verifier_model_len = 0 }

typedef struct hu_gvr_check_result {
    hu_gvr_verdict_t verdict;
    char *critique;       /* heap-allocated; NULL when PASS. Caller frees via alloc. */
    size_t critique_len;
} hu_gvr_check_result_t;

typedef struct hu_gvr_pipeline_result {
    hu_gvr_verdict_t final_verdict;
    uint32_t revisions_performed;
    char *final_content;       /* heap-allocated revised content (or original if PASS). */
    size_t final_content_len;
} hu_gvr_pipeline_result_t;

/* Verify a response: calls provider with verifier system prompt, parses PASS/FAIL + critique. */
hu_error_t hu_gvr_check(hu_allocator_t *alloc, hu_provider_t *provider,
                        const char *model, size_t model_len,
                        const char *original_prompt, size_t original_prompt_len,
                        const char *response, size_t response_len,
                        hu_gvr_check_result_t *out);

/* Revise a response given a critique. */
hu_error_t hu_gvr_revise(hu_allocator_t *alloc, hu_provider_t *provider,
                         const char *model, size_t model_len,
                         const char *original_prompt, size_t original_prompt_len,
                         const char *response, size_t response_len,
                         const char *critique, size_t critique_len,
                         char **revised_out, size_t *revised_out_len);

/* Run the full GVR pipeline: check -> (revise -> re-check) up to max_revisions. */
hu_error_t hu_gvr_pipeline(hu_allocator_t *alloc, hu_provider_t *provider,
                           const hu_gvr_config_t *config,
                           const char *generator_model, size_t generator_model_len,
                           const char *original_prompt, size_t original_prompt_len,
                           const char *initial_response, size_t initial_response_len,
                           hu_gvr_pipeline_result_t *out);

void hu_gvr_check_result_free(hu_allocator_t *alloc, hu_gvr_check_result_t *result);
void hu_gvr_pipeline_result_free(hu_allocator_t *alloc, hu_gvr_pipeline_result_t *result);

#endif /* HU_AGENT_GVR_H */
