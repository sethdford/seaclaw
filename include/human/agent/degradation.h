#ifndef HU_AGENT_DEGRADATION_H
#define HU_AGENT_DEGRADATION_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory/vector/circuit_breaker.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_degrade_strategy {
    HU_DEGRADE_PRIMARY = 0,
    HU_DEGRADE_FALLBACK,
    HU_DEGRADE_HONEST_FAILURE,
} hu_degrade_strategy_t;

typedef struct hu_provider_degradation_config {
    bool enabled;
    char *fallback_model;           /* simpler/cheaper model to try on primary failure */
    size_t fallback_model_len;
    uint32_t max_retries;           /* per-model retry count (default 1) */
    hu_circuit_breaker_t breaker;   /* embedded; caller inits via hu_circuit_breaker_init */
} hu_provider_degradation_config_t;

typedef struct hu_degradation_result {
    hu_chat_response_t response;
    hu_degrade_strategy_t strategy_used;
    uint32_t attempts;
} hu_degradation_result_t;

/* Try primary model, fall back to fallback model, then honest failure.
 * When disabled or config is NULL, behaves identically to a direct provider->vtable->chat. */
hu_error_t hu_provider_degrade_chat(hu_provider_degradation_config_t *config,
                                    hu_provider_t *provider, hu_allocator_t *alloc,
                                    const hu_chat_request_t *request,
                                    const char *model, size_t model_len,
                                    double temperature,
                                    hu_degradation_result_t *out);

/* Generate the honest failure message content (heap-allocated). */
char *hu_degradation_honest_failure_msg(hu_allocator_t *alloc, size_t *out_len);

#endif /* HU_AGENT_DEGRADATION_H */
