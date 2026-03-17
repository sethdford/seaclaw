#ifndef HU_EVAL_JUDGE_H
#define HU_EVAL_JUDGE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_eval_judge_result {
    int score;
    bool passed;
    double raw_score;
    char *reasoning;
    size_t reasoning_len;
} hu_eval_judge_result_t;

#define HU_EVAL_JUDGE_CACHE_SLOTS 64

typedef struct hu_eval_judge_cache_entry {
    uint64_t hash;
    bool occupied;
    hu_eval_judge_result_t result;
} hu_eval_judge_cache_entry_t;

typedef struct hu_eval_judge_cache {
    hu_allocator_t *alloc;
    hu_eval_judge_cache_entry_t slots[HU_EVAL_JUDGE_CACHE_SLOTS];
} hu_eval_judge_cache_t;

hu_error_t hu_eval_judge_check(hu_allocator_t *alloc, hu_provider_t *provider,
                                const char *model, size_t model_len,
                                const char *question, size_t question_len,
                                const char *actual, size_t actual_len,
                                const char *expected, size_t expected_len,
                                const char *rubric, size_t rubric_len,
                                int pass_threshold,
                                hu_eval_judge_cache_t *cache,
                                hu_eval_judge_result_t *out);

void hu_eval_judge_result_free(hu_allocator_t *alloc, hu_eval_judge_result_t *result);

hu_error_t hu_eval_judge_cache_create(hu_allocator_t *alloc, hu_eval_judge_cache_t *cache);
bool hu_eval_judge_cache_lookup(hu_eval_judge_cache_t *cache, const char *actual,
                                 size_t actual_len, const char *expected,
                                 size_t expected_len, const char *rubric,
                                 size_t rubric_len, hu_eval_judge_result_t *out);
void hu_eval_judge_cache_destroy(hu_eval_judge_cache_t *cache);

#endif
