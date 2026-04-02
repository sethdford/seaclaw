#ifndef HU_AGENT_COMPACTION_STRUCTURED_H
#define HU_AGENT_COMPACTION_STRUCTURED_H

#include "human/agent.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Structured Compaction — XML summaries, artifact pinning, continuation
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_MAX_ARTIFACT_PINS       32
#define HU_MAX_TOOL_MENTIONS       64
#define HU_MAX_RECENT_REQUESTS     3
#define HU_CONTINUATION_PREAMBLE   \
    "This session is being continued from a previous conversation that was " \
    "summarized due to context window limits."

typedef struct hu_compaction_summary {
    uint32_t total_messages;
    uint32_t preserved_count;
    uint32_t summarized_count;
    char **tool_mentions;       /* owned array of owned strings */
    size_t tool_mentions_count;
    char *recent_user_requests;       /* concatenated last N user messages */
    size_t recent_user_requests_len;
    char *pending_work_inference;     /* inferred outstanding tasks */
    size_t pending_work_inference_len;
} hu_compaction_summary_t;

typedef struct hu_artifact_pin {
    char *file_path;            /* owned */
    size_t file_path_len;
    uint64_t last_modified_ts;
} hu_artifact_pin_t;

/* Free all owned memory inside a compaction summary. */
void hu_compaction_summary_free(hu_allocator_t *alloc, hu_compaction_summary_t *summary);

/* Free an array of artifact pins. */
void hu_artifact_pins_free(hu_allocator_t *alloc, hu_artifact_pin_t *pins, size_t count);

/* ── Summary generation ──────────────────────────────────────────────────
 * Build an XML structured summary from compacted messages + metadata.
 * Output: <summary> XML with message counts, tool mentions, recent requests,
 * pending work. Strips <analysis> blocks from input content.
 * Caller frees *out_xml via alloc->free(ctx, ptr, *out_xml_len + 1). */
hu_error_t hu_compact_build_structured_summary(
    hu_allocator_t *alloc,
    const hu_owned_message_t *messages, size_t count,
    const hu_compaction_summary_t *metadata,
    char **out_xml, size_t *out_xml_len);

/* ── Metadata extraction ─────────────────────────────────────────────────
 * Scan message history and populate a compaction_summary_t with tool mentions,
 * recent user requests, and pending-work inference. preserve_recent: number
 * of recent messages that will be kept (not summarized). */
hu_error_t hu_compact_extract_metadata(
    hu_allocator_t *alloc,
    const hu_owned_message_t *messages, size_t count,
    uint32_t preserve_recent,
    hu_compaction_summary_t *out);

/* ── Artifact detection ──────────────────────────────────────────────────
 * Scan messages for references to files in workspace_dir. Returns pinned
 * artifact list (max HU_MAX_ARTIFACT_PINS). Caller frees via
 * hu_artifact_pins_free(). */
hu_error_t hu_compact_detect_artifacts(
    hu_allocator_t *alloc,
    const hu_owned_message_t *messages, size_t count,
    const char *workspace_dir, size_t workspace_dir_len,
    hu_artifact_pin_t **out_pins, size_t *out_count);

/* ── Artifact pinning check ──────────────────────────────────────────────
 * Returns true if the message at msg_index references any pinned artifact,
 * meaning it should be preserved (not compacted). */
bool hu_compact_is_pinned(
    const hu_owned_message_t *msg,
    const hu_artifact_pin_t *pins, size_t pin_count);

/* ── Continuation preamble ───────────────────────────────────────────────
 * Inject a continuation preamble message at the beginning of history
 * (after system prompt if present). The preamble includes the structured
 * summary context. Returns HU_OK on success. */
hu_error_t hu_compact_inject_continuation_preamble(
    hu_allocator_t *alloc,
    const hu_compaction_summary_t *summary,
    hu_owned_message_t *history, size_t *history_count, size_t *history_cap);

/* ── Strip analysis blocks ───────────────────────────────────────────────
 * Remove <analysis>...</analysis> blocks from content string in-place.
 * Returns new length. */
size_t hu_compact_strip_analysis(char *content, size_t len);

#endif /* HU_AGENT_COMPACTION_STRUCTURED_H */
