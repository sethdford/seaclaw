#ifndef HU_PROVIDERS_APPLE_H
#define HU_PROVIDERS_APPLE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>

/* Default apfel server endpoint (OpenAI-compatible). */
#define HU_APPLE_DEFAULT_BASE_URL "http://127.0.0.1:11434/v1"
#define HU_APPLE_MODEL_NAME "apple-foundationmodel"
#define HU_APPLE_CONTEXT_WINDOW 4096

typedef struct hu_apple_config {
    const char *base_url;
    size_t base_url_len;
} hu_apple_config_t;

hu_error_t hu_apple_provider_create(hu_allocator_t *alloc, const hu_apple_config_t *config,
                                    hu_provider_t *out);

/* Probe whether an apfel server is reachable at the given base URL (or default).
 * Returns true if the /models endpoint responds with a 200. */
bool hu_apple_probe(hu_allocator_t *alloc, const char *base_url, size_t base_url_len);

#endif /* HU_PROVIDERS_APPLE_H */
