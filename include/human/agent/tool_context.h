#ifndef HU_AGENT_TOOL_CONTEXT_H
#define HU_AGENT_TOOL_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct hu_agent hu_agent_t;

/* Set/clear the current agent for tool execution (thread-local).
 * Called by agent loop at turn start/end so tools like send_message can access
 * the calling agent (mailbox, agent id). */
void hu_agent_set_current_for_tools(hu_agent_t *agent);
void hu_agent_clear_current_for_tools(void);
hu_agent_t *hu_agent_get_current_for_tools(void);

/* Pending voice message — set by send_voice_message tool, consumed by daemon.
 * Thread-local; cleared automatically when agent context is cleared. */
void hu_agent_request_voice_send(const char *emotion, const char *transcript,
                                 size_t transcript_len);
bool hu_agent_has_pending_voice(void);
const char *hu_agent_pending_voice_emotion(void);
const char *hu_agent_pending_voice_transcript(size_t *out_len);
void hu_agent_clear_pending_voice(void);

#endif /* HU_AGENT_TOOL_CONTEXT_H */
