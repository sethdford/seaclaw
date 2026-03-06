#ifndef SC_AGENT_COMPACTION_H
#define SC_AGENT_COMPACTION_H

#include "seaclaw/agent.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Agent Compaction — context window management
 * ────────────────────────────────────────────────────────────────────────── */

typedef struct sc_compaction_config {
    uint32_t keep_recent;          /* keep this many most-recent messages */
    uint32_t max_summary_chars;    /* max chars in compaction summary */
    uint32_t max_source_chars;     /* max chars from source when building summary */
    uint64_t token_limit;          /* 0 = no token-based trigger */
    uint32_t max_history_messages; /* message count trigger */
} sc_compaction_config_t;

/* Default config values */
#define SC_COMPACTION_DEFAULT_KEEP_RECENT       20
#define SC_COMPACTION_DEFAULT_MAX_SUMMARY_CHARS 2000
#define SC_COMPACTION_DEFAULT_MAX_SOURCE_CHARS  12000
#define SC_COMPACTION_DEFAULT_TOKEN_LIMIT       32000
#define SC_COMPACTION_DEFAULT_MAX_HISTORY       50

/* Initialize config with defaults. */
void sc_compaction_config_default(sc_compaction_config_t *cfg);

/* Estimate total tokens in history: (sum(content_len) + 3*count) / 4. */
uint64_t sc_estimate_tokens(const sc_owned_message_t *history, size_t history_count);

/* Check if history exceeds token or message limits and should be compacted. */
bool sc_should_compact(const sc_owned_message_t *history, size_t history_count,
                       const sc_compaction_config_t *config);

/* Compact history: keep N most recent, summarize older into single message.
 * Summary is concatenation of key points (role: content) — not LLM-generated.
 * Modifies history in place. Returns SC_OK on success. */
sc_error_t sc_compact_history(sc_allocator_t *alloc, sc_owned_message_t *history,
                              size_t *history_count, size_t *history_cap,
                              const sc_compaction_config_t *config);

/* LLM-enhanced compaction: sends old messages to the provider for summarization.
 * Falls back to rule-based compaction if provider is NULL or the LLM call fails.
 * provider/alloc/history semantics are the same as sc_compact_history. */
sc_error_t sc_compact_history_llm(sc_allocator_t *alloc, sc_owned_message_t *history,
                                  size_t *history_count, size_t *history_cap,
                                  const sc_compaction_config_t *config, sc_provider_t *provider);

/* Pressure-based compaction: remove oldest non-system messages until pressure < target.
 * Replaces removed messages with "[Previous context compacted: N messages summarized]".
 * Preserves system prompt and most recent messages. */
sc_error_t sc_context_compact_for_pressure(sc_allocator_t *alloc, sc_owned_message_t *history,
                                           size_t *history_count, size_t *history_cap,
                                           size_t max_tokens, float target_pressure);

#endif /* SC_AGENT_COMPACTION_H */
