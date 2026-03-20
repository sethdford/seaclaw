#ifndef HU_TOOLS_VISUAL_GROUNDING_H
#define HU_TOOLS_VISUAL_GROUNDING_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"

hu_error_t hu_visual_ground_action(hu_allocator_t *alloc, hu_provider_t *provider,
                                   const char *model, size_t model_len,
                                   const char *screenshot_path, size_t path_len,
                                   const char *action_description, size_t action_len,
                                   double *out_x, double *out_y, char **out_selector,
                                   size_t *out_selector_len);

#endif
