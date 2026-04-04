#ifndef HU_AGENT_DEFINITION_H
#define HU_AGENT_DEFINITION_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct hu_agent_definition {
    char *soul_name; /* from SOUL.md frontmatter: name */
    char *soul_voice; /* from SOUL.md frontmatter: voice */
    char **soul_traits; /* from SOUL.md frontmatter: traits[] */
    size_t soul_traits_count;
    char *soul_body; /* SOUL.md body text */

    char **rules; /* from RULES.md: each ## section */
    size_t rules_count;

    char *memory_context; /* from MEMORY.md body */

    char **enabled_tools; /* from TOOLS.md */
    size_t enabled_tools_count;
} hu_agent_definition_t;

hu_error_t hu_agent_definition_load(hu_allocator_t *alloc, const char *workspace_dir,
                                    hu_agent_definition_t *out);
void hu_agent_definition_deinit(hu_agent_definition_t *def, hu_allocator_t *alloc);

#endif
