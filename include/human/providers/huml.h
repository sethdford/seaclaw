#ifndef HU_HUML_PROVIDER_H
#define HU_HUML_PROVIDER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"

typedef struct hu_huml_config {
    const char *checkpoint_path;
    size_t checkpoint_path_len;
    size_t max_tokens;
} hu_huml_config_t;

hu_error_t hu_huml_provider_create(hu_allocator_t *alloc, const hu_huml_config_t *config,
                                   hu_provider_t *out);

#endif
