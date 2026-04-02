#ifndef HU_USAGE_H
#define HU_USAGE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Per-provider token usage and cost tracking (extended for cache tokens)
 * ────────────────────────────────────────────────────────────────────────── */

typedef struct hu_extended_token_usage {
    uint64_t input_tokens;
    uint64_t output_tokens;
    uint64_t cache_read_tokens;
    uint64_t cache_write_tokens;
} hu_extended_token_usage_t;

typedef struct hu_model_pricing {
    const char *model_pattern;    /* e.g. "claude-*", "gpt-4*" */
    double input_per_mtok;        /* $ per million input tokens */
    double output_per_mtok;       /* $ per million output tokens */
    double cache_read_per_mtok;   /* $ per million cache read tokens (optional) */
    double cache_write_per_mtok;  /* $ per million cache write tokens (optional) */
} hu_model_pricing_t;

typedef struct hu_model_usage {
    char model_name[256];
    uint64_t input_tokens;
    uint64_t output_tokens;
    uint64_t cache_read_tokens;
    uint64_t cache_write_tokens;
    double estimated_cost_usd;
    size_t request_count;
} hu_model_usage_t;

typedef struct hu_usage_tracker hu_usage_tracker_t;

/* Create a new usage tracker */
hu_error_t hu_usage_tracker_create(hu_allocator_t *alloc, hu_usage_tracker_t **out);

/* Destroy tracker and free resources */
void hu_usage_tracker_destroy(hu_usage_tracker_t *tracker);

/* Record usage from a single API call (with provider and model name) */
hu_error_t hu_usage_tracker_record(hu_usage_tracker_t *tracker,
                                    const char *model, const hu_extended_token_usage_t *usage);

/* Get totals across all models */
hu_error_t hu_usage_tracker_get_totals(const hu_usage_tracker_t *tracker,
                                        hu_extended_token_usage_t *out);

/* Get estimated cost in USD */
double hu_usage_tracker_estimate_cost(const hu_usage_tracker_t *tracker);

/* Get per-model breakdown (array of hu_model_usage_t, caller frees) */
hu_error_t hu_usage_tracker_get_breakdown(const hu_usage_tracker_t *tracker,
                                          hu_allocator_t *alloc,
                                          hu_model_usage_t **out, size_t *out_count);

/* Format a human-readable report for /cost command output */
hu_error_t hu_usage_tracker_format_report(const hu_usage_tracker_t *tracker,
                                           hu_allocator_t *alloc,
                                           char **out, size_t *out_len);

/* Reset all counters */
void hu_usage_tracker_reset(hu_usage_tracker_t *tracker);

/* Get request count */
size_t hu_usage_tracker_request_count(const hu_usage_tracker_t *tracker);

#endif /* HU_USAGE_H */
