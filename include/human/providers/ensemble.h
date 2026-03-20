#ifndef HU_PROVIDERS_ENSEMBLE_H
#define HU_PROVIDERS_ENSEMBLE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"

#define HU_ENSEMBLE_MAX_PROVIDERS 8

typedef enum {
    HU_ENSEMBLE_ROUND_ROBIN = 0,
    HU_ENSEMBLE_BEST_FOR_TASK,
    HU_ENSEMBLE_CONSENSUS,
} hu_ensemble_strategy_t;

typedef struct hu_ensemble_config {
    hu_provider_t providers[HU_ENSEMBLE_MAX_PROVIDERS];
    size_t provider_count;
    hu_ensemble_strategy_t strategy;
} hu_ensemble_config_t;

/*
 * Composite provider: routes chat/chat_with_system across sub-providers.
 * On success, the ensemble owns sub-providers until deinit — deinit calls each child's deinit.
 */
hu_error_t hu_ensemble_create(hu_allocator_t *alloc, const hu_ensemble_config_t *config,
                              hu_provider_t *out);

#endif
