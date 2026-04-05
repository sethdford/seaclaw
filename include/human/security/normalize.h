#ifndef HU_SECURITY_NORMALIZE_H
#define HU_SECURITY_NORMALIZE_H

#include "human/core/error.h"
#include <stddef.h>

hu_error_t hu_normalize_confusables(const char *input, size_t input_len,
                                     char *out, size_t out_cap, size_t *out_len);

#endif
