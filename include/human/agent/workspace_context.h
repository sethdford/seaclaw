#ifndef HU_AGENT_WORKSPACE_CONTEXT_H
#define HU_AGENT_WORKSPACE_CONTEXT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Workspace context analysis — detect project type and extract metadata
 * ────────────────────────────────────────────────────────────────────────── */

typedef struct hu_workspace_context {
    char *project_type;       /* "nodejs", "rust", "go", "python", "c", "unknown" */
    size_t project_type_len;
    char *project_name;       /* extracted from config file */
    size_t project_name_len;
    char *summary;            /* e.g. "Node.js project 'myapp' v1.2.3 with scripts: build, test" */
    size_t summary_len;
} hu_workspace_context_t;

/* Detect project type from workspace_dir by checking for standard config files.
 * Returns project metadata in *out. Caller must free with hu_workspace_context_free.
 * Returns HU_OK on success (type may be "unknown" if no config found). */
hu_error_t hu_workspace_context_detect(hu_allocator_t *alloc, const char *workspace_dir,
                                       hu_workspace_context_t *out);

/* Free all owned strings in context. Safe to call on zero-initialized struct. */
void hu_workspace_context_free(hu_allocator_t *alloc, hu_workspace_context_t *ctx);

#endif /* HU_AGENT_WORKSPACE_CONTEXT_H */
