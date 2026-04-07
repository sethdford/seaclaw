#ifndef HU_AGENT_HUMANNESS_H
#define HU_AGENT_HUMANNESS_H

#include "human/core/error.h"
#include <stddef.h>

typedef struct hu_agent hu_agent_t;

/* Build conversation_context for CLI-like paths that lack daemon-level
 * humanness wiring (life_sim, mood, voice_maturity, style_clone, time-of-day).
 * No-op when conversation_context is already set (daemon path). */
hu_error_t hu_agent_build_turn_context(hu_agent_t *agent);

/* Free conversation_context if it was built by the humanness module. */
void hu_agent_free_turn_context(hu_agent_t *agent);

/* Update voice profile after a completed turn (CLI path). */
hu_error_t hu_agent_update_voice_profile(hu_agent_t *agent, const char *user_msg, size_t msg_len);

#endif /* HU_AGENT_HUMANNESS_H */
