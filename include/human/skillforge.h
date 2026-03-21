#ifndef HU_SKILLFORGE_H
#define HU_SKILLFORGE_H

#include "core/allocator.h"
#include "core/error.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Skill — a discovered skill with manifest metadata.
 * Level-1 (catalog): name + description stay small for system prompts.
 * Level-2: instructions live in SKILL.md on disk; instructions_path points to that file.
 * Level-3: bundled files under skill_dir (NULL for flat *.skill.json layouts).
 */
typedef struct hu_skill {
    char *name;
    char *description;
    char *command;    /* shell command to execute, may be NULL */
    char *parameters; /* JSON string, may be NULL */
    char *skill_dir;  /* skill bundle directory, or NULL for flat .skill.json */
    char *instructions_path; /* path to SKILL.md, or NULL */
    bool enabled;
} hu_skill_t;

/** Sentinel SKILL.md path used with HU_IS_TEST discover data (see tests). */
#define HU_SKILLFORGE_TEST_INSTRUCTIONS_PATH "__HU_TEST_SKILL_MD__"

/**
 * SkillForge — skill discovery and integration registry.
 */
typedef struct hu_skillforge {
    hu_skill_t *skills;
    size_t skills_len;
    size_t skills_cap;
    hu_allocator_t *alloc;
} hu_skillforge_t;

hu_error_t hu_skillforge_create(hu_allocator_t *alloc, hu_skillforge_t *out);
void hu_skillforge_destroy(hu_skillforge_t *sf);

/**
 * Discover skills by scanning a directory for *.skill.json files.
 * In HU_IS_TEST mode, uses test data instead of scanning.
 */
hu_error_t hu_skillforge_discover(hu_skillforge_t *sf, const char *dir_path);

/**
 * Lookup a skill by name. Returns NULL if not found.
 */
hu_skill_t *hu_skillforge_get_skill(const hu_skillforge_t *sf, const char *name);

/**
 * List all registered skills. Output is owned by sf; valid until destroy.
 */
hu_error_t hu_skillforge_list_skills(const hu_skillforge_t *sf, hu_skill_t **out,
                                     size_t *out_count);

/**
 * Count keyword token overlaps between user_msg and skill name+description (same tokenization
 * as top_k catalog ranking). Used for dynamic skill routing weights.
 */
int hu_skillforge_skill_keyword_hits(const hu_skill_t *skill, const char *user_msg,
                                     size_t user_msg_len);

/**
 * Build the "## Available Skills" catalog lines (name + description per enabled skill).
 * When getenv("HUMAN_SKILLS_CONTEXT") is "top_k" and there are more than top_k skills,
 * keeps the K best keyword matches against user_msg (HUMAN_SKILLS_TOP_K, default 12).
 * Caller frees *out with alloc. Empty registry returns HU_OK with *out NULL.
 */
hu_error_t hu_skillforge_build_prompt_catalog(hu_allocator_t *alloc, hu_skillforge_t *sf,
                                              const char *user_msg, size_t user_msg_len,
                                              char **out, size_t *out_len);

hu_error_t hu_skillforge_enable(hu_skillforge_t *sf, const char *name);
hu_error_t hu_skillforge_disable(hu_skillforge_t *sf, const char *name);

hu_error_t hu_skillforge_execute(hu_allocator_t *alloc, const hu_skillforge_t *sf, const char *name,
                                 char **out_instructions);

/**
 * Load level-2 instructions: SKILL.md body (frontmatter stripped), or description if no path.
 * In HU_IS_TEST, never reads the filesystem (mock path HU_SKILLFORGE_TEST_INSTRUCTIONS_PATH).
 * @param out_len optional; when non-NULL, set to strlen(result).
 */
hu_error_t hu_skillforge_load_instructions(hu_allocator_t *alloc, const hu_skill_t *skill,
                                           char **out_instructions, size_t *out_len);

/**
 * Load a bundled resource file from skill_dir/name (level 3). Validates resource_name.
 * In HU_IS_TEST, uses mock data only (no filesystem).
 * @param out_len optional; when non-NULL, set to content length (excluding NUL).
 */
hu_error_t hu_skillforge_read_resource(hu_allocator_t *alloc, const hu_skill_t *skill,
                                       const char *resource_name, char **out_content, size_t *out_len);

hu_error_t hu_skillforge_install(const char *name, const char *url);

hu_error_t hu_skillforge_uninstall(hu_skillforge_t *sf, const char *name);

#endif /* HU_SKILLFORGE_H */
