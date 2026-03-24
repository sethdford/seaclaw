#ifndef HU_AGENT_PROMPT_OPTIMIZER_H
#define HU_AGENT_PROMPT_OPTIMIZER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * DSPy-style programmatic prompt optimization.
 * Defines prompt signatures (inputs/outputs/constraints), runs optimization
 * against a golden set, and emits optimized prompts.
 * Source: DSPy 3.x (Stanford)
 */

typedef struct hu_prompt_field {
    char name[64];
    char description[256];
    bool required;
} hu_prompt_field_t;

#define HU_PROMPT_MAX_FIELDS 16

typedef struct hu_prompt_signature {
    char name[128];
    hu_prompt_field_t inputs[HU_PROMPT_MAX_FIELDS];
    size_t input_count;
    hu_prompt_field_t outputs[HU_PROMPT_MAX_FIELDS];
    size_t output_count;
    char instruction[1024];
} hu_prompt_signature_t;

typedef struct hu_prompt_example {
    const char *input;
    size_t input_len;
    const char *expected_output;
    size_t expected_output_len;
    double score; /* 0.0-1.0; from eval */
} hu_prompt_example_t;

#define HU_PROMPT_MAX_EXAMPLES 32

typedef struct hu_prompt_optimizer {
    hu_prompt_signature_t signature;
    hu_prompt_example_t examples[HU_PROMPT_MAX_EXAMPLES];
    size_t example_count;
    char optimized_prompt[4096];
    size_t optimized_prompt_len;
    double best_score;
    uint32_t iterations_run;
    uint32_t max_iterations; /* 0 = default (5) */
} hu_prompt_optimizer_t;

void hu_prompt_optimizer_init(hu_prompt_optimizer_t *opt);

hu_error_t hu_prompt_signature_add_input(hu_prompt_signature_t *sig, const char *name,
                                         const char *description, bool required);

hu_error_t hu_prompt_signature_add_output(hu_prompt_signature_t *sig, const char *name,
                                          const char *description, bool required);

hu_error_t hu_prompt_optimizer_add_example(hu_prompt_optimizer_t *opt,
                                           const hu_prompt_example_t *example);

hu_error_t hu_prompt_optimizer_compile(hu_prompt_optimizer_t *opt, char *out_prompt,
                                       size_t out_size, size_t *out_len);

hu_error_t hu_prompt_optimizer_optimize(hu_prompt_optimizer_t *opt, hu_allocator_t *alloc,
                                        hu_provider_t *provider, const char *model,
                                        size_t model_len, const hu_prompt_example_t *golden_set,
                                        size_t golden_count);

#endif /* HU_AGENT_PROMPT_OPTIMIZER_H */
