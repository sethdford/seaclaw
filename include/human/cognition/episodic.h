#ifndef HU_COGNITION_EPISODIC_H
#define HU_COGNITION_EPISODIC_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_episodic_pattern {
    char *id;              /* UUID string; owned */
    char *problem_type;    /* classification label; owned */
    char *approach;        /* compact description; owned */
    char *skills_used;     /* comma-separated skill names; owned */
    float outcome_quality; /* 0.0–1.0 */
    uint32_t support_count;
    char *insight;         /* one-line takeaway; owned */
    char *session_id;      /* originating session; owned */
    char *timestamp;       /* ISO 8601; owned */
} hu_episodic_pattern_t;

/* Input for pattern extraction from a completed session */
typedef struct hu_episodic_session_summary {
    const char *session_id;
    size_t session_id_len;
    const char *const *tool_names;   /* tools used during session */
    size_t tool_count;
    const char *const *skill_names;  /* skills invoked via skill_run */
    size_t skill_count;
    bool had_positive_feedback;
    bool had_correction;
    const char *topic;               /* primary topic of session; NULL ok */
    size_t topic_len;
} hu_episodic_session_summary_t;

/* Create the episodic_patterns table. Idempotent. */
hu_error_t hu_episodic_init_schema(sqlite3 *db);

/* Extract patterns from a session summary and store them.
 * Heuristic extraction (no LLM call). */
hu_error_t hu_episodic_extract_and_store(sqlite3 *db, hu_allocator_t *alloc,
                                          const hu_episodic_session_summary_t *summary);

/* Retrieve matching patterns for a query string.
 * Returns up to max_results patterns sorted by relevance * quality * recency.
 * Caller owns the array; free with hu_episodic_free_patterns. */
hu_error_t hu_episodic_retrieve(sqlite3 *db, hu_allocator_t *alloc,
                                 const char *query, size_t query_len,
                                 size_t max_results,
                                 hu_episodic_pattern_t **out, size_t *out_count);

/* Build a markdown "Cognitive Replay" block from retrieved patterns.
 * Returns HU_OK with *out=NULL if no patterns. Caller owns string. */
hu_error_t hu_episodic_build_replay(hu_allocator_t *alloc,
                                     const hu_episodic_pattern_t *patterns,
                                     size_t count,
                                     char **out, size_t *out_len);

/* Compress patterns with the same problem_type (merge 3+ into one abstract pattern). */
hu_error_t hu_episodic_compress(sqlite3 *db, hu_allocator_t *alloc);

/* Free a pattern array. */
void hu_episodic_free_patterns(hu_allocator_t *alloc,
                                hu_episodic_pattern_t *patterns, size_t count);

/* Store a single pattern. Exposed for testing. */
hu_error_t hu_episodic_store_pattern(sqlite3 *db, hu_allocator_t *alloc,
                                      const hu_episodic_pattern_t *pattern);

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_COGNITION_EPISODIC_H */
