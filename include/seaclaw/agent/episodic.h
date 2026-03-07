#ifndef SC_AGENT_EPISODIC_H
#define SC_AGENT_EPISODIC_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/memory.h"
#include "seaclaw/provider.h"
#include <stddef.h>
#include <stdint.h>

#define SC_EPISODIC_KEY_PREFIX     "_ep:"
#define SC_EPISODIC_KEY_PREFIX_LEN 4
#define SC_EPISODIC_MAX_SUMMARY    512
#define SC_EPISODIC_MAX_LOAD       5

typedef struct sc_episode {
    char *summary;
    size_t summary_len;
    uint64_t timestamp_ms; /* Unix epoch milliseconds */
    char *session_id;
    size_t session_id_len;
} sc_episode_t;

/* Build a short session summary from the conversation for episodic storage.
 * messages is an array of alternating user/assistant content strings.
 * Caller owns the returned string. */
char *sc_episodic_summarize_session(sc_allocator_t *alloc, const char *const *messages,
                                    const size_t *message_lens, size_t message_count,
                                    size_t *out_len);

/* LLM-based session summarization — calls the provider to produce a concise
 * 2-3 sentence summary. Falls back to rule-based on provider failure.
 * Caller owns the returned string. */
char *sc_episodic_summarize_session_llm(sc_allocator_t *alloc, sc_provider_t *provider,
                                        const char *const *messages, const size_t *message_lens,
                                        size_t message_count, size_t *out_len);

/* Store an episode into the memory backend. */
sc_error_t sc_episodic_store(sc_memory_t *memory, sc_allocator_t *alloc, const char *session_id,
                             size_t session_id_len, const char *summary, size_t summary_len);

/* Load recent episodes as formatted context for prompt injection.
 * Caller owns the returned string. */
sc_error_t sc_episodic_load(sc_memory_t *memory, sc_allocator_t *alloc, char **out,
                            size_t *out_len);

#endif /* SC_AGENT_EPISODIC_H */
