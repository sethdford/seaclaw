#ifndef HU_PERSONA_MARKDOWN_LOADER_H
#define HU_PERSONA_MARKDOWN_LOADER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/persona.h"
#include <stddef.h>

/* Load a persona definition from a markdown file with YAML frontmatter.
 * Frontmatter fields: name, identity, traits (array), model, temperature,
 * tools (array), channels (array), overlays (key: {formality, avg_length}).
 * Body text (after second ---) becomes/appends to identity. */
hu_error_t hu_persona_load_markdown(hu_allocator_t *alloc, const char *path,
                                    hu_persona_t *out);

/* Discover agent markdown definitions in a directory.
 * Scans for *.md files with valid frontmatter in agents_dir.
 * Returns array of names (caller frees each + array). */
hu_error_t hu_persona_discover_agents(hu_allocator_t *alloc, const char *agents_dir,
                                      char ***out_names, size_t *out_count);

#endif /* HU_PERSONA_MARKDOWN_LOADER_H */
