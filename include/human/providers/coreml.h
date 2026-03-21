#ifndef HU_PROVIDERS_COREML_H
#define HU_PROVIDERS_COREML_H

#include "human/provider.h"
#include <stddef.h>

typedef struct hu_coreml_config {
    const char *model_path;
    size_t model_path_len;
} hu_coreml_config_t;

hu_error_t hu_coreml_provider_create(hu_allocator_t *alloc, const hu_coreml_config_t *config,
                                     hu_provider_t *out);

#endif
