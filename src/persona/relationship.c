/*
 * Relationship depth tracker — session-based warmth and formality adaptation.
 */
#include "seaclaw/persona/relationship.h"
#include "seaclaw/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *STAGE_NAMES[] = {"new", "familiar", "trusted", "deep"};

static const char *STAGE_GUIDANCE[] = {
    "This is a newer relationship. Be helpful, clear, and professional. Build trust through "
    "reliability.",
    "You know this user moderately well. Reference past conversations when relevant. Be warmer.",
    "This is a trusted relationship. Be candid and proactive. Share observations and insights "
    "freely.",
    "This is a deep, long-standing relationship. Be genuinely present. Anticipate needs. "
    "Celebrate growth.",
};

void sc_relationship_update(sc_relationship_state_t *state, uint32_t turn_count) {
    if (!state)
        return;
    state->session_count++;
    state->total_turns += turn_count;

    if (state->session_count >= 50)
        state->stage = SC_REL_DEEP;
    else if (state->session_count >= 20)
        state->stage = SC_REL_TRUSTED;
    else if (state->session_count >= 5)
        state->stage = SC_REL_FAMILIAR;
    else
        state->stage = SC_REL_NEW;
}

sc_error_t sc_relationship_build_prompt(sc_allocator_t *alloc,
                                         const sc_relationship_state_t *state,
                                         char **out, size_t *out_len) {
    if (!alloc || !state || !out || !out_len)
        return SC_ERR_INVALID_ARGUMENT;

    const char *stage_name = STAGE_NAMES[(size_t)state->stage];
    const char *guidance = STAGE_GUIDANCE[(size_t)state->stage];

#define SC_REL_BUF_CAP 256
    char *buf = (char *)alloc->alloc(alloc->ctx, SC_REL_BUF_CAP);
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;

    int n = snprintf(buf, SC_REL_BUF_CAP,
                     "\n### Relationship Context\nStage: %s. Sessions: %u. %s\n", stage_name,
                     state->session_count, guidance);
    if (n <= 0 || (size_t)n >= SC_REL_BUF_CAP) {
        alloc->free(alloc->ctx, buf, SC_REL_BUF_CAP);
        return SC_ERR_INVALID_ARGUMENT;
    }

    size_t need = (size_t)n + 1;
    char *shrunk = (char *)alloc->realloc(alloc->ctx, buf, SC_REL_BUF_CAP, need);
    if (!shrunk) {
        alloc->free(alloc->ctx, buf, SC_REL_BUF_CAP);
        return SC_ERR_OUT_OF_MEMORY;
    }
    *out = shrunk;
    *out_len = (size_t)n;
#undef SC_REL_BUF_CAP
    return SC_OK;
}
