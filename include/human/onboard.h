#ifndef HU_ONBOARD_H
#define HU_ONBOARD_H

#include "core/allocator.h"
#include "core/error.h"
#include <stdbool.h>

typedef struct hu_onboard_opts {
    const char *api_key;
    const char *provider;
    const char *model;
    const char *channel;
    const char *workspace;
    bool non_interactive;
    bool start_gateway;
} hu_onboard_opts_t;

/**
 * Setup wizard: provider, API key, model, optional messaging channel, write config.
 * Uses stdin/stdout unless `opts->non_interactive` is true.
 * With NULL `opts`, behaves as fully interactive with default flags.
 * In HU_IS_TEST mode, returns HU_OK immediately without prompting.
 */
hu_error_t hu_onboard_run_with_opts(hu_allocator_t *alloc, const hu_onboard_opts_t *opts);

/**
 * Equivalent to `hu_onboard_run_with_opts(alloc, NULL)`.
 */
hu_error_t hu_onboard_run(hu_allocator_t *alloc);

/**
 * Check if this is the first run (no ~/.human/config.json exists).
 */
bool hu_onboard_check_first_run(void);

#endif /* HU_ONBOARD_H */
