#ifndef SC_ACTION_PREVIEW_H
#define SC_ACTION_PREVIEW_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>

typedef struct sc_action_preview {
    const char *tool_name;
    char *description;      /* owned */
    const char *risk_level; /* "low", "medium", "high" */
} sc_action_preview_t;

sc_error_t sc_action_preview_generate(sc_allocator_t *alloc, const char *tool_name,
                                      const char *args_json, size_t args_json_len,
                                      sc_action_preview_t *out);
sc_error_t sc_action_preview_format(sc_allocator_t *alloc, const sc_action_preview_t *p, char **out,
                                    size_t *out_len);
void sc_action_preview_free(sc_allocator_t *alloc, sc_action_preview_t *p);

#endif
