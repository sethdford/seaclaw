#ifndef HU_AGENT_CHAOS_H
#define HU_AGENT_CHAOS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Chaos testing: inject faults to verify agent resilience.
 * Wraps providers and tools with configurable failure injection
 * for dirty data, slow responses, partial failures, and timeouts.
 */

typedef enum hu_chaos_fault {
    HU_CHAOS_NONE = 0,
    HU_CHAOS_TIMEOUT,          /* simulate timeout */
    HU_CHAOS_EMPTY_RESPONSE,   /* return empty content */
    HU_CHAOS_GARBAGE_RESPONSE, /* return malformed content */
    HU_CHAOS_PARTIAL_RESPONSE, /* truncate response */
    HU_CHAOS_ERROR_CODE,       /* return an error */
    HU_CHAOS_SLOW_RESPONSE,    /* simulate latency */
    HU_CHAOS_CORRUPT_JSON,     /* return broken JSON */
    HU_CHAOS_FAULT_COUNT,
} hu_chaos_fault_t;

typedef struct hu_chaos_config {
    bool enabled;
    double fault_probability;      /* 0.0-1.0 */
    hu_chaos_fault_t forced_fault; /* HU_CHAOS_NONE = random */
    uint32_t seed;                 /* deterministic PRNG seed */
} hu_chaos_config_t;

#define HU_CHAOS_CONFIG_DEFAULT \
    {.enabled = false, .fault_probability = 0.1, .forced_fault = HU_CHAOS_NONE, .seed = 42}

typedef struct hu_chaos_stats {
    size_t total_calls;
    size_t faults_injected;
    size_t timeouts;
    size_t empty_responses;
    size_t garbage_responses;
    size_t partial_responses;
    size_t error_codes;
    size_t slow_responses;
    size_t corrupt_json;
} hu_chaos_stats_t;

typedef struct hu_chaos_engine {
    hu_chaos_config_t config;
    hu_chaos_stats_t stats;
    uint32_t prng_state; /* xorshift32 state */
} hu_chaos_engine_t;

void hu_chaos_engine_init(hu_chaos_engine_t *e, const hu_chaos_config_t *config);

hu_chaos_fault_t hu_chaos_maybe_inject(hu_chaos_engine_t *e);

hu_error_t hu_chaos_apply_to_response(hu_chaos_engine_t *e, hu_allocator_t *alloc,
                                      hu_chaos_fault_t fault, hu_chat_response_t *resp);

hu_error_t hu_chaos_apply_to_tool_result(hu_chaos_engine_t *e, hu_allocator_t *alloc,
                                         hu_chaos_fault_t fault, hu_tool_result_t *result);

size_t hu_chaos_report(const hu_chaos_engine_t *e, char *buf, size_t buf_size);

const char *hu_chaos_fault_name(hu_chaos_fault_t fault);

#endif /* HU_AGENT_CHAOS_H */
