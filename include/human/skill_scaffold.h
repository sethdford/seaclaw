#ifndef HU_SKILL_SCAFFOLD_H
#define HU_SKILL_SCAFFOLD_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/*
 * Skill scaffolding — generate template skill projects with
 * manifest, SKILL.md, and optional test harness.
 *
 * `human skills init <name>` creates a ready-to-use skill project:
 *   <name>/
 *     manifest.json      — skill metadata
 *     SKILL.md           — human-readable instructions
 *     README.md          — quick start
 *
 * Categories determine which tool parameters and prompt patterns
 * are pre-populated in the template.
 */

typedef enum hu_skill_category {
    HU_SKILL_CATEGORY_GENERAL = 0,
    HU_SKILL_CATEGORY_DATA,
    HU_SKILL_CATEGORY_AUTOMATION,
    HU_SKILL_CATEGORY_INTEGRATION,
    HU_SKILL_CATEGORY_ANALYSIS,
} hu_skill_category_t;

typedef struct hu_skill_scaffold_opts {
    const char *name;
    const char *description;
    const char *author;
    hu_skill_category_t category;
    const char *output_dir; /* NULL defaults to current directory */
} hu_skill_scaffold_opts_t;

/* Generate a skill project scaffold on disk.
 * Under HU_IS_TEST, validates inputs and returns HU_OK without filesystem. */
hu_error_t hu_skill_scaffold_init(hu_allocator_t *alloc, const hu_skill_scaffold_opts_t *opts);

/* Return a manifest.json template string (caller frees). */
hu_error_t hu_skill_scaffold_manifest(hu_allocator_t *alloc, const hu_skill_scaffold_opts_t *opts,
                                      char **out, size_t *out_len);

/* Return a SKILL.md template string (caller frees). */
hu_error_t hu_skill_scaffold_instructions(hu_allocator_t *alloc,
                                          const hu_skill_scaffold_opts_t *opts, char **out,
                                          size_t *out_len);

#endif /* HU_SKILL_SCAFFOLD_H */
