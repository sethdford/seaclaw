#ifndef HU_AGENT_HULA_LITE_H
#define HU_AGENT_HULA_LITE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/* Indentation-based HuLa syntax (subset). Lines use 2 spaces per indent level.
 * Supported:
 *   program <name>
 *   seq <id> | par <id>
 *   call <id> <tool>
 *     <arg_key> <arg value rest of line>
 * Nested seq/par under a composite is allowed (one extra level).
 * Returns JSON suitable for hu_hula_parse_json. Caller frees *out. */
hu_error_t hu_hula_lite_to_json(hu_allocator_t *alloc, const char *src, size_t src_len, char **out,
                                size_t *out_len);

#endif /* HU_AGENT_HULA_LITE_H */
