#ifndef HU_PROVIDERS_ENSEMBLE_H
#define HU_PROVIDERS_ENSEMBLE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"

/*
 * Hybrid local + cloud routing (config.json)
 * ------------------------------------------
 * Use the ensemble composite as `default_provider` (or per-agent provider) to combine a local
 * backend with a cloud API. Example:
 *
 *   {
 *     "default_provider": "ensemble",
 *     "ensemble": {
 *       "providers": ["ollama", "anthropic"],
 *       "strategy": "best_for_task",
 *       "routing": {
 *         "simple": "ollama",
 *         "complex": "anthropic"
 *       }
 *     }
 *   }
 *
 * Strategies map to hu_ensemble_strategy_t: "round_robin", "best_for_task", "consensus".
 * `best_for_task` uses keyword heuristics on the user message to pick among `providers`
 * (e.g. code-ish prompts bias toward names containing "openai" / "anthropic").
 *
 * The `routing` object is reserved for explicit simple/complex overrides; it is accepted in
 * JSON (see config schema `ensemble.routing`) for forward-compatible configs but is not yet
 * applied by the runtime — keep your preferred default first in `providers` until then.
 */

#define HU_ENSEMBLE_MAX_PROVIDERS 8

typedef enum {
    HU_ENSEMBLE_ROUND_ROBIN = 0,
    HU_ENSEMBLE_BEST_FOR_TASK,
    HU_ENSEMBLE_CONSENSUS,
} hu_ensemble_strategy_t;

typedef struct hu_ensemble_spec {
    hu_provider_t providers[HU_ENSEMBLE_MAX_PROVIDERS];
    size_t provider_count;
    hu_ensemble_strategy_t strategy;
} hu_ensemble_spec_t;

/*
 * Composite provider: routes chat/chat_with_system across sub-providers.
 * On success, the ensemble owns sub-providers until deinit — deinit calls each child's deinit.
 */
hu_error_t hu_ensemble_create(hu_allocator_t *alloc, const hu_ensemble_spec_t *config,
                              hu_provider_t *out);

#endif
