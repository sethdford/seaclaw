#ifndef HU_SKILL_REGISTRY_H
#define HU_SKILL_REGISTRY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

#define HU_SKILL_REGISTRY_URL \
    "https://raw.githubusercontent.com/human/skill-registry/main/registry.json"

/**
 * Registry entry — one skill from the remote registry index.
 */
typedef struct hu_skill_registry_entry {
    char *name;
    char *description;
    char *version;
    char *author;
    char *url;
    char *sha256; /* lowercase hex bundle digest from index, or NULL */
    char *tags; /* comma-separated, or NULL */
} hu_skill_registry_entry_t;

/**
 * Search the registry index by query. Filters by name, description, tags.
 * @param registry_url HTTPS URL of registry.json, or NULL for HU_SKILL_REGISTRY_URL.
 * Under HU_IS_TEST, returns mock data without network.
 * Caller frees entries with hu_skill_registry_entries_free.
 */
hu_error_t hu_skill_registry_search(hu_allocator_t *alloc, const char *registry_url,
                                    const char *query, hu_skill_registry_entry_t **out_entries,
                                    size_t *out_count);

void hu_skill_registry_entries_free(hu_allocator_t *alloc, hu_skill_registry_entry_t *entries,
                                    size_t count);

/**
 * Install a skill from a local source path. Copies manifest and associated
 * files to ~/.human/skills/<skill_name>/. Source must contain .skill.json,
 * manifest.json, or <name>.skill.json.
 * Under HU_IS_TEST, returns HU_OK without filesystem.
 */
hu_error_t hu_skill_registry_install(hu_allocator_t *alloc, const char *source_path);

/**
 * Install a skill from the remote registry by exact name. Resolves the entry URL,
 * fetches <name>.skill.json and optional SKILL.md from raw.githubusercontent.com,
 * and writes ~/.human/skills/<name>/manifest.json (+ SKILL.md when present).
 * @param registry_url same semantics as hu_skill_registry_search.
 * Under HU_IS_TEST, validates arguments and returns HU_OK without network.
 */
hu_error_t hu_skill_registry_install_by_name(hu_allocator_t *alloc, const char *registry_url,
                                             const char *name);

/**
 * Compare installed skill directory digest to registry sha256 for @a name.
 */
hu_error_t hu_skill_registry_verify(hu_allocator_t *alloc, const char *registry_url,
                                    const char *name);

/**
 * Reinstall from registry when remote version is newer than installed manifest version.
 */
hu_error_t hu_skill_registry_upgrade(hu_allocator_t *alloc, const char *registry_url,
                                     const char *name);

/**
 * Uninstall (remove) an installed skill from disk.
 * Removes ~/.human/skills/<name>/ and ~/.human/skills/<name>.skill.json.
 * Under HU_IS_TEST, returns HU_OK without filesystem.
 */
hu_error_t hu_skill_registry_uninstall(const char *name);

/**
 * Re-install a skill from source path (uninstall then install).
 * Under HU_IS_TEST, returns HU_OK without filesystem.
 */
hu_error_t hu_skill_registry_update(hu_allocator_t *alloc, const char *source_path);

/**
 * Get the installed skills directory path (~/.human/skills).
 * Writes to out, returns length written. Returns 0 if home not set.
 */
size_t hu_skill_registry_get_installed_dir(char *out, size_t out_len);

/**
 * Publish a skill from a directory (validates .skill.json or SKILL.md, prints
 * contribute instructions). Under HU_IS_TEST, returns HU_OK without filesystem.
 */
hu_error_t hu_skill_registry_publish(hu_allocator_t *alloc, const char *skill_dir);

#endif /* HU_SKILL_REGISTRY_H */
