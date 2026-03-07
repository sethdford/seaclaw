#ifndef SC_AGENT_OUTCOMES_H
#define SC_AGENT_OUTCOMES_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Outcome Tracker — records tool results and user corrections to enable
 * continuous learning. Feeds into persona feedback and pattern detection.
 * ────────────────────────────────────────────────────────────────────────── */

#define SC_OUTCOME_MAX_RECENT 64

typedef enum sc_outcome_type {
    SC_OUTCOME_TOOL_SUCCESS,
    SC_OUTCOME_TOOL_FAILURE,
    SC_OUTCOME_USER_CORRECTION,
    SC_OUTCOME_USER_POSITIVE, /* explicit approval or thanks */
} sc_outcome_type_t;

typedef struct sc_outcome_entry {
    sc_outcome_type_t type;
    uint64_t timestamp_ms;
    char tool_name[64];
    char summary[256];
} sc_outcome_entry_t;

typedef struct sc_outcome_tracker {
    sc_outcome_entry_t entries[SC_OUTCOME_MAX_RECENT]; /* circular buffer */
    size_t write_idx;
    size_t total;
    uint64_t tool_successes;
    uint64_t tool_failures;
    uint64_t corrections;
    uint64_t positives;
    bool auto_apply_feedback;
} sc_outcome_tracker_t;

/* Initialize an outcome tracker. */
void sc_outcome_tracker_init(sc_outcome_tracker_t *tracker, bool auto_apply_feedback);

/* Record a tool execution outcome. */
void sc_outcome_record_tool(sc_outcome_tracker_t *tracker, const char *tool_name, bool success,
                            const char *summary);

/* Record a user correction (detected from message content). */
void sc_outcome_record_correction(sc_outcome_tracker_t *tracker, const char *original,
                                  const char *correction);

/* Record positive feedback (thanks, approval). */
void sc_outcome_record_positive(sc_outcome_tracker_t *tracker, const char *context);

/* Get recent entries for pattern analysis. Returns pointer to internal buffer.
 * count is set to number of valid entries (up to SC_OUTCOME_MAX_RECENT). */
const sc_outcome_entry_t *sc_outcome_get_recent(const sc_outcome_tracker_t *tracker, size_t *count);

/* Build a summary string of outcome stats. Caller owns returned string. */
char *sc_outcome_build_summary(const sc_outcome_tracker_t *tracker, sc_allocator_t *alloc,
                               size_t *out_len);

/* Detect repeated patterns: returns true if the same tool has failed >= threshold times recently.
 */
bool sc_outcome_detect_repeated_failure(const sc_outcome_tracker_t *tracker, const char *tool_name,
                                        size_t threshold);

#endif /* SC_AGENT_OUTCOMES_H */
