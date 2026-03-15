#ifndef HU_EMBEDDED_PROVIDER_H
#define HU_EMBEDDED_PROVIDER_H
#include "human/provider.h"
typedef struct hu_embedded_config { char *model_path; size_t context_size; int threads; bool use_gpu; } hu_embedded_config_t;
hu_error_t hu_embedded_provider_create(hu_allocator_t *alloc, const hu_embedded_config_t *config, hu_provider_t *out);
#endif
