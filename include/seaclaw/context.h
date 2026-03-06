#ifndef SC_CONTEXT_H
#define SC_CONTEXT_H

#include "seaclaw/agent.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

char *sc_context_build_system_prompt(sc_allocator_t *alloc, const char *base, size_t base_len,
                                     const char *workspace_dir, size_t workspace_dir_len);

sc_error_t sc_context_format_messages(sc_allocator_t *alloc, const sc_owned_message_t *history,
                                      size_t history_count, size_t max_messages,
                                      sc_chat_message_t **out_messages, size_t *out_count);

uint32_t sc_context_estimate_tokens(const sc_chat_message_t *messages, size_t messages_count);

/* Estimate tokens for a single text string (rough: ~4 chars per token). */
size_t sc_estimate_tokens_text(const char *text, size_t len);

/* Context pressure tracking for agent loop */
typedef struct sc_context_pressure {
    size_t current_tokens;
    size_t max_tokens;
    float pressure;
    bool warning_85_emitted;
    bool warning_95_emitted;
} sc_context_pressure_t;

/* Update pressure from current_tokens and max_tokens. Emit warnings at configured
 * thresholds. Returns true if auto-compact should be triggered (>= pressure_compact). */
bool sc_context_check_pressure(sc_context_pressure_t *p, float pressure_warn,
                               float pressure_compact);

#endif /* SC_CONTEXT_H */
