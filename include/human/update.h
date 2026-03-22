#ifndef HU_UPDATE_H
#define HU_UPDATE_H

#include "human/core/error.h"
#include <stddef.h>

/* Self-update mechanism.
 * hu_update_check: queries GitHub releases via curl for newer versions.
 * hu_update_apply: downloads and replaces the binary (Unix only).
 * Requires HU_ENABLE_CURL; returns HU_ERR_NOT_SUPPORTED otherwise. */
hu_error_t hu_update_check(char *version_buf, size_t buf_size);
hu_error_t hu_update_apply(void);

#endif /* HU_UPDATE_H */
