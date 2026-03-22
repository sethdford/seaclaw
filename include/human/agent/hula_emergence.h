#ifndef HU_AGENT_HULA_EMERGENCE_H
#define HU_AGENT_HULA_EMERGENCE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

struct hu_skillforge;

/* Write a trace record JSON file under dir (or ~/.human/hula_traces when dir is NULL).
 * No-op in HU_IS_TEST when dir is NULL.
 * When program_json is non-NULL, embeds it under "program" so `human hula replay` can re-execute. */
hu_error_t hu_hula_trace_persist(hu_allocator_t *alloc, const char *trace_dir,
                                 const char *trace_json, size_t trace_json_len,
                                 const char *program_name, size_t program_name_len, bool success,
                                 const char *program_json, size_t program_json_len);

/* Scan JSON trace files in trace_dir; find n-grams of tool names (length ngram_len) that
 * appear at least min_occurrences times across files. Caller frees *out_patterns and *out_counts. */
hu_error_t hu_hula_emergence_scan(hu_allocator_t *alloc, const char *trace_dir, size_t ngram_len,
                                  size_t min_occurrences, char ***out_patterns, size_t *out_pattern_count,
                                  size_t **out_freqs);

/* Promote one pattern "a|b|c" into a SkillForge bundle under skills_dir (or ~/.human/skills).
 * Creates manifest-style skill with HuLa JSON in SKILL.md body. */
hu_error_t hu_hula_emergence_promote(hu_allocator_t *alloc, const char *skills_dir,
                                     const char *pattern, size_t pattern_len,
                                     const char *skill_name, size_t skill_name_len);

#endif /* HU_AGENT_HULA_EMERGENCE_H */
