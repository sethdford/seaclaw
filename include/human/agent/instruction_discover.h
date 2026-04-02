#ifndef HU_AGENT_INSTRUCTION_DISCOVER_H
#define HU_AGENT_INSTRUCTION_DISCOVER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Instruction file discovery — .human.md / HUMAN.md / instructions.md
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_INSTRUCTION_MAX_CHARS_PER_FILE 4000
#define HU_INSTRUCTION_MAX_CHARS_TOTAL 12000
#define HU_INSTRUCTION_MAX_WALK_LEVELS 10
#define HU_INSTRUCTION_CACHE_TTL_SEC 300 /* 5 minutes */
#define HU_INSTRUCTION_MAX_VISITED_INODES 64

typedef enum hu_instruction_source {
    HU_INSTRUCTION_SOURCE_USER_HOME,    /* ~/.human/instructions.md */
    HU_INSTRUCTION_SOURCE_PROJECT_ROOT, /* HUMAN.md in ancestor dirs */
    HU_INSTRUCTION_SOURCE_WORKSPACE,    /* <workspace>/.human.md */
} hu_instruction_source_t;

typedef struct hu_instruction_file {
    hu_instruction_source_t source;
    char *path;         /* canonical path; owned */
    size_t path_len;
    char *content;      /* owned; truncated to limit */
    size_t content_len;
    bool truncated;     /* true if file exceeded per-file limit */
    int64_t mtime;      /* modification time (epoch seconds) */
} hu_instruction_file_t;

typedef struct hu_instruction_discovery {
    hu_instruction_file_t *files; /* owned array */
    size_t file_count;
    char *merged_content;         /* owned; priority-ordered merge */
    size_t merged_content_len;
    int64_t last_check_time;      /* epoch seconds of last freshness check */
} hu_instruction_discovery_t;

/* Discover instruction files starting from workspace_dir.
 * Discovery order (high → low priority):
 *   1. <workspace_dir>/.human.md
 *   2. Walk upward from workspace_dir for HUMAN.md (max 10 levels)
 *   3. ~/.human/instructions.md
 * Caller owns the returned discovery; free with hu_instruction_discovery_destroy. */
hu_error_t hu_instruction_discovery_run(hu_allocator_t *alloc,
                                        const char *workspace_dir,
                                        size_t workspace_dir_len,
                                        hu_instruction_discovery_t **out);

/* Check if cached discovery is still fresh (all mtimes unchanged, within TTL).
 * Returns true if cache is valid and no re-discovery is needed. */
bool hu_instruction_discovery_is_fresh(const hu_instruction_discovery_t *disc);

/* Free all resources held by a discovery result. */
void hu_instruction_discovery_destroy(hu_allocator_t *alloc,
                                      hu_instruction_discovery_t *disc);

/* Read a single instruction file with per-file char limit.
 * path must be canonical (use realpath). Sets truncated flag. */
hu_error_t hu_instruction_file_read(hu_allocator_t *alloc,
                                    const char *path,
                                    hu_instruction_source_t source,
                                    hu_instruction_file_t *out);

/* Merge instruction files in priority order into a single string.
 * Files are ordered: workspace (highest), project root, user home (lowest).
 * Total output is capped at HU_INSTRUCTION_MAX_CHARS_TOTAL. */
hu_error_t hu_instruction_merge(hu_allocator_t *alloc,
                                const hu_instruction_file_t *files,
                                size_t file_count,
                                char **out,
                                size_t *out_len);

/* Validate a path for safety: no null bytes, canonicalize with realpath.
 * Returns HU_OK if path is safe; writes canonical path to out_canonical.
 * Caller owns out_canonical (allocated with alloc). */
hu_error_t hu_instruction_validate_path(hu_allocator_t *alloc,
                                        const char *path,
                                        size_t path_len,
                                        char **out_canonical,
                                        size_t *out_canonical_len);

#endif /* HU_AGENT_INSTRUCTION_DISCOVER_H */
